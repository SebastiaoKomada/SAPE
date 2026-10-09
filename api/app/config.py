from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql+asyncpg://sape:sape@localhost:5432/sape"

    mqtt_enabled: bool = True
    mqtt_host: str = "broker.hivemq.com"
    mqtt_port: int = 1883
    mqtt_topic: str = "sape/+/telemetria"

    device_offline_after_min: int = 20
    rise_rate_window_min: int = 60


@lru_cache
def get_settings() -> Settings:
    return Settings()
