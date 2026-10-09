from datetime import UTC, datetime, timedelta
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, Response, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.ext.asyncio import AsyncSession

from app.config import get_settings
from app.db import get_session
from app.models import Device, Measurement
from app.schemas import DeviceCreate, DeviceOut, DeviceUpdate, MeasurementOut

router = APIRouter(prefix="/devices", tags=["devices"])

Session = Annotated[AsyncSession, Depends(get_session)]


async def get_device(device_id: int, session: Session) -> Device:
    device = await session.get(Device, device_id)
    if device is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Dispositivo não encontrado")
    return device


def is_online(last_seen_at: datetime | None) -> bool:
    if last_seen_at is None:
        return False
    limit = timedelta(minutes=get_settings().device_offline_after_min)
    return datetime.now(UTC) - last_seen_at <= limit


async def _latest_by_device(session: AsyncSession, device_ids: list[int]) -> dict[int, Measurement]:
    if not device_ids:
        return {}
    rows = await session.scalars(
        select(Measurement)
        .where(Measurement.device_id.in_(device_ids))
        .distinct(Measurement.device_id)
        .order_by(Measurement.device_id, Measurement.measured_at.desc())
    )
    return {m.device_id: m for m in rows}


def _to_out(device: Device, latest: Measurement | None) -> DeviceOut:
    return DeviceOut(
        id=device.id,
        name=device.name,
        identifier=device.identifier,
        location=device.location,
        sensor_height_m=device.sensor_height_m,
        status=device.status,
        last_seen_at=device.last_seen_at,
        online=is_online(device.last_seen_at),
        created_at=device.created_at,
        updated_at=device.updated_at,
        latest_measurement=MeasurementOut.model_validate(latest) if latest else None,
    )


async def _device_out(session: AsyncSession, device: Device) -> DeviceOut:
    latest = await _latest_by_device(session, [device.id])
    return _to_out(device, latest.get(device.id))


async def _commit_unique(session: AsyncSession) -> None:
    try:
        await session.commit()
    except IntegrityError:
        await session.rollback()
        raise HTTPException(status.HTTP_409_CONFLICT, "Já existe um dispositivo com esse identificador")


@router.get("", response_model=list[DeviceOut])
async def list_devices(session: Session):
    devices = list(await session.scalars(select(Device).order_by(Device.name)))
    latest = await _latest_by_device(session, [d.id for d in devices])
    return [_to_out(d, latest.get(d.id)) for d in devices]


@router.post("", response_model=DeviceOut, status_code=status.HTTP_201_CREATED)
async def create_device(body: DeviceCreate, session: Session):
    device = Device(**body.model_dump())
    session.add(device)
    await _commit_unique(session)
    await session.refresh(device)
    return _to_out(device, None)


@router.get("/{device_id}", response_model=DeviceOut)
async def read_device(device: Annotated[Device, Depends(get_device)], session: Session):
    return await _device_out(session, device)


@router.patch("/{device_id}", response_model=DeviceOut)
async def update_device(
    body: DeviceUpdate, device: Annotated[Device, Depends(get_device)], session: Session
):
    # A nova altura do sensor vale só para as próximas leituras; o histórico não muda.
    for field, value in body.model_dump(exclude_unset=True).items():
        if value is None and field in {"name", "identifier", "status"}:
            continue
        setattr(device, field, value)
    await _commit_unique(session)
    await session.refresh(device)
    return await _device_out(session, device)


@router.delete("/{device_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_device(device: Annotated[Device, Depends(get_device)], session: Session):
    # As medições são apagadas em cascata pela FK
    await session.delete(device)
    await session.commit()
    return Response(status_code=status.HTTP_204_NO_CONTENT)
