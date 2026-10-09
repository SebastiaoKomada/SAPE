from datetime import UTC, datetime, timedelta
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.analysis import choose_bucket_seconds, rise_rate_cm_per_h
from app.config import get_settings
from app.db import get_session
from app.models import Device, Measurement
from app.routers.devices import get_device
from app.schemas import History, HistoryPoint, MeasurementOut, Stats

router = APIRouter(prefix="/devices/{device_id}", tags=["measurements"])

Session = Annotated[AsyncSession, Depends(get_session)]
DeviceDep = Annotated[Device, Depends(get_device)]

MAX_HISTORY_SPAN = timedelta(days=366)


@router.get("/measurements/latest", response_model=MeasurementOut | None)
async def latest_measurement(device: DeviceDep, session: Session):
    return await session.scalar(
        select(Measurement)
        .where(Measurement.device_id == device.id)
        .order_by(Measurement.measured_at.desc())
        .limit(1)
    )


@router.get("/measurements/recent", response_model=list[MeasurementOut])
async def recent_measurements(
    device: DeviceDep, session: Session, limit: Annotated[int, Query(ge=1, le=200)] = 20
):
    """Últimas leituras brutas, incluindo as falhas do sensor."""
    rows = await session.scalars(
        select(Measurement)
        .where(Measurement.device_id == device.id)
        .order_by(Measurement.measured_at.desc())
        .limit(limit)
    )
    return list(rows)


@router.get("/measurements", response_model=History)
async def measurement_history(
    device: DeviceDep,
    session: Session,
    start: datetime | None = None,
    end: datetime | None = None,
    bucket: Annotated[int | None, Query(ge=1, description="Força o intervalo de agregação (s)")] = None,
):
    """Histórico das leituras válidas, agregado para caber em ~300 pontos."""
    end = _aware(end) if end else datetime.now(UTC)
    start = _aware(start) if start else end - timedelta(hours=24)
    if start >= end:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_CONTENT, "start deve ser anterior a end")
    if end - start > MAX_HISTORY_SPAN:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_CONTENT, "Período máximo é de 366 dias")

    in_range = (
        Measurement.device_id == device.id,
        Measurement.sensor_ok.is_(True),
        Measurement.measured_at >= start,
        Measurement.measured_at < end,
    )

    if bucket is None:
        count = await session.scalar(select(func.count()).where(*in_range))
        bucket = choose_bucket_seconds((end - start).total_seconds(), count or 0)

    if bucket is None:
        rows = await session.execute(
            select(Measurement.measured_at, Measurement.river_level_m, Measurement.distance_m)
            .where(*in_range)
            .order_by(Measurement.measured_at)
        )
        points = [
            HistoryPoint(
                t=t, level_avg=lvl, level_min=lvl, level_max=lvl,
                distance_avg=dist, distance_min=dist, distance_max=dist,
            )
            for t, lvl, dist in rows
        ]
    else:
        epoch = func.extract("epoch", Measurement.measured_at)
        bucket_start = func.to_timestamp(func.floor(epoch / bucket) * bucket).label("t")
        rows = await session.execute(
            select(
                bucket_start,
                func.avg(Measurement.river_level_m),
                func.min(Measurement.river_level_m),
                func.max(Measurement.river_level_m),
                func.avg(Measurement.distance_m),
                func.min(Measurement.distance_m),
                func.max(Measurement.distance_m),
            )
            .where(*in_range)
            .group_by(bucket_start)
            .order_by(bucket_start)
        )
        points = [
            HistoryPoint(
                t=t, level_avg=la, level_min=lmin, level_max=lmax,
                distance_avg=da, distance_min=dmin, distance_max=dmax,
            )
            for t, la, lmin, lmax, da, dmin, dmax in rows
        ]

    return History(start=start, end=end, bucket_s=bucket, points=points)


@router.get("/stats", response_model=Stats)
async def device_stats(
    device: DeviceDep,
    session: Session,
    window_min: Annotated[int | None, Query(ge=1, le=7 * 24 * 60)] = None,
):
    window_min = window_min or get_settings().rise_rate_window_min
    since = datetime.now(UTC) - timedelta(minutes=window_min)

    rows = (
        await session.execute(
            select(
                Measurement.measured_at,
                Measurement.distance_m,
                Measurement.river_level_m,
                Measurement.sensor_ok,
            )
            .where(Measurement.device_id == device.id, Measurement.measured_at >= since)
            .order_by(Measurement.measured_at)
        )
    ).all()
    valid = [r for r in rows if r.sensor_ok]

    # Leitura válida mais recente, mesmo que esteja fora da janela
    current = await session.execute(
        select(Measurement.distance_m, Measurement.river_level_m)
        .where(Measurement.device_id == device.id, Measurement.sensor_ok.is_(True))
        .order_by(Measurement.measured_at.desc())
        .limit(1)
    )
    current_row = current.first()

    # Taxa e variação vêm da distância (subir o nível = diminuir a distância), o que
    # funciona sem altura cadastrada e não é afetado por troca de altura na janela.
    rate = rise_rate_cm_per_h([(r.measured_at, -r.distance_m) for r in valid])
    change = valid[0].distance_m - valid[-1].distance_m if len(valid) >= 2 else None

    return Stats(
        window_min=window_min,
        readings_in_window=len(rows),
        failed_in_window=len(rows) - len(valid),
        current_distance_m=current_row.distance_m if current_row else None,
        current_level_m=current_row.river_level_m if current_row else None,
        level_change_m=change,
        rise_rate_cm_h=rate,
    )


def _aware(dt: datetime) -> datetime:
    return dt if dt.tzinfo else dt.replace(tzinfo=UTC)
