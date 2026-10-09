import asyncio
import contextlib
import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI

from app.config import get_settings
from app.ingest.subscriber import run_subscriber
from app.routers import health

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Rode a API com um único worker: cada worker abriria sua própria assinatura
    # e as leituras seriam gravadas em duplicidade.
    task = asyncio.create_task(run_subscriber()) if get_settings().mqtt_enabled else None
    yield
    if task:
        task.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await task


app = FastAPI(title="SAPE API", lifespan=lifespan)
app.include_router(health.router, prefix="/api")
