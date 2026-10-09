# SAPE — Sistema de Alerta Preditivo de Enchentes

Monitoramento do nível do Rio Sapucaí. A estação (ESP32 + sensor A02YYUW) publica as leituras
via MQTT; a API grava no PostgreSQL e o dashboard web mostra nível, tendência e saúde do equipamento.

```
firmware/  sketch Arduino da estação (LilyGO T-A7670SA)
api/       FastAPI + PostgreSQL + assinante MQTT
web/       React + Vite + shadcn/ui
```

## Fluxo dos dados

1. A estação publica em `sape/<DEVICE_ID>/telemetria` no broker `broker.hivemq.com:1883`:
   `{"distancia_m": 1.234, "sensor_ok": true, "bateria_v": 4.01, "solar_v": 5.10}`
2. A API assina `sape/+/telemetria`, valida a mensagem e grava em `measurements` — só de
   dispositivos **cadastrados e ativos**. Leituras com falha do sensor também são gravadas.
3. Nível do rio = altura do sensor (cadastrada no dispositivo) − distância medida.
4. O dashboard consulta a API a cada 10 s.

## Como rodar (desenvolvimento)

Pré-requisitos: Python 3.12+, Node 20+, Docker.

### 1. Banco de dados

```bash
cd api && docker compose up -d
```

### 2. API

```bash
cd api
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
cp .env.example .env
.venv/bin/alembic upgrade head
.venv/bin/fastapi dev app/main.py
```

API em http://localhost:8000 e documentação interativa em http://localhost:8000/docs.
Rode sempre com **um único worker**: o assinante MQTT roda dentro da API, e cada worker
gravaria as leituras em duplicidade.

Testes:

```bash
cd api && .venv/bin/python -m pytest
```

### 3. Frontend

```bash
cd web
npm install
npm run dev
```

Dashboard em http://localhost:5173 (o Vite encaminha `/api` para a porta 8000).

### 4. Firmware

1. Copie `firmware/sape/secrets.example.h` para `firmware/sape/secrets.h` e preencha Wi-Fi e
   `DEVICE_ID` (o `secrets.h` não é versionado).
2. Abra `firmware/sape/sape.ino` na Arduino IDE (biblioteca **PubSubClient**) e grave na placa.
3. No dashboard, em **Dispositivos**, cadastre a estação com o mesmo identificador do `DEVICE_ID`
   e a altura do sensor em relação ao zero do rio.

## Configuração (`api/.env`)

| Variável | Padrão | Descrição |
|---|---|---|
| `DATABASE_URL` | `postgresql+asyncpg://sape:sape@localhost:5432/sape` | Conexão com o banco |
| `MQTT_ENABLED` | `true` | Liga/desliga o assinante MQTT |
| `MQTT_HOST` / `MQTT_PORT` | `broker.hivemq.com` / `1883` | Broker |
| `MQTT_TOPIC` | `sape/+/telemetria` | Tópico assinado |
| `DEVICE_OFFLINE_AFTER_MIN` | `20` | Sem mensagem há mais que isso → offline |
| `RISE_RATE_WINDOW_MIN` | `60` | Janela da taxa de elevação |

## Rotas da API

| Método | Rota | Descrição |
|---|---|---|
| GET | `/api/health` | Status da API |
| GET, POST | `/api/devices` | Lista (com online e última leitura) / cadastra |
| GET, PATCH, DELETE | `/api/devices/{id}` | Detalhe / edita / exclui (apaga as leituras) |
| GET | `/api/devices/{id}/measurements/latest` | Leitura mais recente |
| GET | `/api/devices/{id}/measurements/recent?limit=20` | Últimas leituras, com falhas |
| GET | `/api/devices/{id}/measurements?start=&end=` | Histórico agregado em até ~300 pontos |
| GET | `/api/devices/{id}/stats?window_min=` | Nível atual, variação e taxa de elevação (cm/h) |

## Limitações conhecidas do MVP

- O broker HiveMQ é público: qualquer pessoa pode publicar no tópico. Só dispositivos cadastrados
  são aceitos, mas um identificador conhecido pode ser forjado. Trocar por um broker próprio com
  autenticação antes de ir a campo.
- O horário da leitura é o de recebimento na API (o firmware ainda não envia timestamp).
- Sem login, alertas por SMS, API de clima ou controle de deep sleep — ficam para a próxima fase.
