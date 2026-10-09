import asyncio
import logging
import uuid
from datetime import UTC, datetime

import aiomqtt
from sqlalchemy import select

from app.config import get_settings
from app.db import SessionLocal
from app.ingest.payload import InvalidPayload, parse_payload, parse_topic, river_level
from app.models import Device, DeviceStatus, Measurement

log = logging.getLogger("sape.ingest")

RECONNECT_DELAY_S = 5


async def handle_message(topic: str, payload: bytes) -> None:
    identifier = parse_topic(topic)
    if identifier is None:
        log.warning("Tópico ignorado: %s", topic)
        return

    try:
        reading = parse_payload(payload)
    except InvalidPayload:
        log.warning("Payload inválido de %s: %r", identifier, payload[:200])
        return

    now = datetime.now(UTC)
    async with SessionLocal() as session:
        device = await session.scalar(select(Device).where(Device.identifier == identifier))
        if device is None:
            log.warning("Dispositivo não cadastrado: %s", identifier)
            return
        if device.status != DeviceStatus.active:
            log.info("Leitura descartada, dispositivo inativo: %s", identifier)
            return

        session.add(
            Measurement(
                device_id=device.id,
                distance_m=reading.distance_m,
                river_level_m=river_level(device.sensor_height_m, reading.distance_m),
                sensor_ok=reading.sensor_ok,
                battery_v=reading.battery_v,
                solar_v=reading.solar_v,
                measured_at=now,
            )
        )
        device.last_seen_at = now
        await session.commit()

    log.info("Leitura gravada: %s %s", identifier, reading)


async def run_subscriber() -> None:
    """Assina o tópico e grava as leituras; reconecta sozinho se o broker cair."""
    settings = get_settings()
    client_id = f"sape-api-{uuid.uuid4().hex[:8]}"

    while True:
        try:
            async with aiomqtt.Client(
                settings.mqtt_host, settings.mqtt_port, identifier=client_id
            ) as client:
                await client.subscribe(settings.mqtt_topic, qos=1)
                log.info("MQTT conectado em %s, assinando %s", settings.mqtt_host, settings.mqtt_topic)
                async for message in client.messages:
                    try:
                        await handle_message(str(message.topic), message.payload)
                    except Exception:
                        log.exception("Erro ao processar mensagem de %s", message.topic)
        except aiomqtt.MqttError as exc:
            log.warning("MQTT desconectado (%s). Nova tentativa em %ss.", exc, RECONNECT_DELAY_S)
            await asyncio.sleep(RECONNECT_DELAY_S)
