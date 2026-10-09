import enum
from datetime import datetime

from sqlalchemy import BigInteger, DateTime, Enum, ForeignKey, Index, String, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db import Base


class DeviceStatus(enum.StrEnum):
    active = "active"
    inactive = "inactive"


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str] = mapped_column(String(100))
    identifier: Mapped[str] = mapped_column(String(100), unique=True)
    location: Mapped[str | None] = mapped_column(String(255))
    # Altura do sensor em relação ao zero do rio; nível = altura - distância
    sensor_height_m: Mapped[float | None]
    status: Mapped[DeviceStatus] = mapped_column(
        Enum(DeviceStatus, name="device_status"), default=DeviceStatus.active
    )
    last_seen_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now()
    )

    measurements: Mapped[list["Measurement"]] = relationship(
        back_populates="device", cascade="all, delete-orphan", passive_deletes=True
    )


class Measurement(Base):
    __tablename__ = "measurements"
    __table_args__ = (Index("ix_measurements_device_time", "device_id", "measured_at"),)

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True)
    device_id: Mapped[int] = mapped_column(ForeignKey("devices.id", ondelete="CASCADE"))
    # Nulos quando a leitura falhou (sensor_ok = false)
    distance_m: Mapped[float | None]
    river_level_m: Mapped[float | None]
    sensor_ok: Mapped[bool]
    battery_v: Mapped[float]
    solar_v: Mapped[float]
    measured_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))

    device: Mapped[Device] = relationship(back_populates="measurements")
