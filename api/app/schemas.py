from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field

from app.models import DeviceStatus

# Vira um nível do tópico MQTT, então não pode conter '/', '+' ou '#'
IDENTIFIER_PATTERN = r"^[a-z0-9][a-z0-9_-]*$"


class MeasurementOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    distance_m: float | None
    river_level_m: float | None
    sensor_ok: bool
    battery_v: float
    solar_v: float
    measured_at: datetime


class DeviceBase(BaseModel):
    name: str = Field(min_length=1, max_length=100)
    location: str | None = Field(default=None, max_length=255)
    sensor_height_m: float | None = Field(default=None, gt=0)
    status: DeviceStatus = DeviceStatus.active


class DeviceCreate(DeviceBase):
    identifier: str = Field(min_length=1, max_length=100, pattern=IDENTIFIER_PATTERN)


class DeviceUpdate(BaseModel):
    name: str | None = Field(default=None, min_length=1, max_length=100)
    identifier: str | None = Field(
        default=None, min_length=1, max_length=100, pattern=IDENTIFIER_PATTERN
    )
    location: str | None = Field(default=None, max_length=255)
    sensor_height_m: float | None = Field(default=None, gt=0)
    status: DeviceStatus | None = None


class DeviceOut(DeviceBase):
    model_config = ConfigDict(from_attributes=True)

    id: int
    identifier: str
    last_seen_at: datetime | None
    online: bool
    created_at: datetime
    updated_at: datetime
    latest_measurement: MeasurementOut | None


class HistoryPoint(BaseModel):
    t: datetime
    level_avg: float | None
    level_min: float | None
    level_max: float | None
    distance_avg: float
    distance_min: float
    distance_max: float


class History(BaseModel):
    start: datetime
    end: datetime
    # None quando os pontos são as leituras brutas
    bucket_s: int | None
    points: list[HistoryPoint]


class Stats(BaseModel):
    window_min: int
    readings_in_window: int
    failed_in_window: int
    current_distance_m: float | None
    current_level_m: float | None
    level_change_m: float | None
    rise_rate_cm_h: float | None
