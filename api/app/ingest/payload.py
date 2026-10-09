"""Interpretação das mensagens MQTT do firmware (funções puras, sem I/O)."""

from dataclasses import dataclass

from pydantic import BaseModel, ValidationError

# Faixa de medição do sensor A02YYUW
DISTANCE_MIN_M = 0.03
DISTANCE_MAX_M = 4.5


class InvalidPayload(ValueError):
    pass


class _FirmwarePayload(BaseModel):
    # Chaves em português, como o firmware envia
    distancia_m: float | None
    sensor_ok: bool
    bateria_v: float
    solar_v: float


@dataclass(frozen=True)
class Reading:
    distance_m: float | None
    sensor_ok: bool
    battery_v: float
    solar_v: float


def parse_topic(topic: str) -> str | None:
    """Extrai o identificador de 'sape/<identifier>/telemetria'."""
    parts = topic.split("/")
    if len(parts) != 3 or parts[0] != "sape" or parts[2] != "telemetria" or not parts[1]:
        return None
    return parts[1]


def parse_payload(raw: bytes | str) -> Reading:
    try:
        data = _FirmwarePayload.model_validate_json(raw)
    except ValidationError as exc:
        raise InvalidPayload(str(exc)) from exc

    distance = data.distancia_m
    ok = data.sensor_ok and distance is not None and DISTANCE_MIN_M <= distance <= DISTANCE_MAX_M
    return Reading(
        distance_m=distance if ok else None,
        sensor_ok=ok,
        battery_v=data.bateria_v,
        solar_v=data.solar_v,
    )


def river_level(sensor_height_m: float | None, distance_m: float | None) -> float | None:
    if sensor_height_m is None or distance_m is None:
        return None
    return sensor_height_m - distance_m
