// SAPE - Apresentação (Modo Contínuo)
// Placa: LilyGO T-A7670SA R2 (ESP32-WROVER-E)

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "secrets.h"  // copie secrets.example.h para secrets.h

// --- Configurações do Wi-Fi ---
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// --- Configurações do MQTT ---
const char* broker = "broker.hivemq.com";
const int   port   = 1883;
const char* topic  = "sape/" DEVICE_ID "/telemetria";

// --- Pinos (LilyGO T-A7670 R2) ---
// Sensor no UART_RX (GPIO3), que é compartilhado com o conversor USB-serial.
// -> Desconecte o fio do sensor ao gravar o código.
// -> Com o cabo USB ligado, as leituras podem falhar; sem USB funciona melhor.
// Se der problema, troque para GPIO 18, 19, 21, 22, 23 ou 32.
constexpr int PINO_RX_SENSOR = 3;
constexpr int PINO_BATERIA   = 35;  // divisor da bateria (1/2)
constexpr int PINO_SOLAR     = 36;  // divisor da entrada solar (1/2) - conferir com multímetro

HardwareSerial sensor(2);
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Temporizadores não bloqueantes
unsigned long ultimaPublicacao   = 0;
unsigned long ultimaTentativaWiFi = 0;
unsigned long ultimaTentativaMqtt = 0;
const unsigned long INTERVALO_ENVIO     = 5000;
const unsigned long INTERVALO_RECONEXAO = 5000;

bool lerSensor(uint16_t &distancia);
float lerTensao(int pino);
bool manterConexoes();

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=======================================");
    Serial.println(" SAPE - Apresentação (Modo Contínuo)");
    Serial.println("=======================================");

    // ADC com faixa completa (~0 a 3,1 V)
    analogSetPinAttenuation(PINO_BATERIA, ADC_11db);
    analogSetPinAttenuation(PINO_SOLAR, ADC_11db);

    // Sensor ultrassônico em modo UART automático (só RX)
    sensor.begin(9600, SERIAL_8N1, PINO_RX_SENSOR, -1);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
    ultimaTentativaWiFi = millis();

    mqtt.setServer(broker, port);
}

void loop() {
    // 1. Mantém Wi-Fi e MQTT sem travar o loop
    bool online = manterConexoes();
    if (online) mqtt.loop();

    // 2. Leitura e envio a cada INTERVALO_ENVIO
    if (millis() - ultimaPublicacao < INTERVALO_ENVIO) return;
    ultimaPublicacao = millis();

    float tensaoBat   = lerTensao(PINO_BATERIA);
    float tensaoSolar = lerTensao(PINO_SOLAR);
    uint16_t distanciaMm = 0;
    bool sensorOk = lerSensor(distanciaMm);

    Serial.println("\n--- Nova Leitura ---");
    Serial.printf("Bateria: %.2f V | Solar: %.2f V\n", tensaoBat, tensaoSolar);
    float distanciaM = distanciaMm / 1000.0f;  // mm -> m

    if (sensorOk) {
        Serial.printf("Distância medida: %.3f m\n", distanciaM);
    } else {
        Serial.println("Aviso: falha ao ler o sensor. Enviando distancia_m = null.");
    }

    // 3. Monta o JSON (null quando o sensor falha, para não confundir com 0 mm)
    char payload[128];
    if (sensorOk) {
        snprintf(payload, sizeof(payload),
                 "{\"distancia_m\": %.3f, \"sensor_ok\": true, \"bateria_v\": %.2f, \"solar_v\": %.2f}",
                 distanciaM, tensaoBat, tensaoSolar);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"distancia_m\": null, \"sensor_ok\": false, \"bateria_v\": %.2f, \"solar_v\": %.2f}",
                 tensaoBat, tensaoSolar);
    }

    Serial.print("Payload: ");
    Serial.println(payload);

    if (!online) {
        Serial.println("Sem conexão: leitura não publicada.");
        return;
    }

    if (mqtt.publish(topic, payload)) {
        Serial.println("Publicado com sucesso.");
    } else {
        Serial.println("Erro ao publicar no MQTT.");
    }
}

// --- Tensão calibrada pelo próprio ESP32 (divisor 1/2 na placa) ---
float lerTensao(int pino) {
    uint32_t somaMv = 0;
    const int AMOSTRAS = 16;
    for (int i = 0; i < AMOSTRAS; i++) {
        somaMv += analogReadMilliVolts(pino);
    }
    float mv = (somaMv / (float)AMOSTRAS) * 2.0f;
    return mv / 1000.0f;
}

// --- Leitura do sensor (quadro: 0xFF, H, L, checksum) ---
bool lerSensor(uint16_t &distancia) {
    // Descarta quadros antigos acumulados no buffer
    while (sensor.available() > 0) sensor.read();

    uint8_t quadro[4];
    uint8_t indice = 0;
    unsigned long inicio = millis();

    // Até 500 ms: o sensor envia um quadro a cada ~100 ms
    while (millis() - inicio < 500) {
        while (sensor.available() > 0) {
            uint8_t b = sensor.read();

            if (indice == 0) {
                if (b == 0xFF) quadro[indice++] = b;
                continue;
            }

            quadro[indice++] = b;
            if (indice == 4) {
                uint8_t checksum = (uint8_t)(quadro[0] + quadro[1] + quadro[2]);
                if (checksum == quadro[3]) {
                    distancia = ((uint16_t)quadro[1] << 8) | quadro[2];
                    return true;
                }
                // Checksum inválido: tenta ressincronizar
                indice = (b == 0xFF) ? 1 : 0;
                if (indice == 1) quadro[0] = 0xFF;
            }
        }
        delay(2);
    }
    return false;
}

// --- Reconexão não bloqueante de Wi-Fi e MQTT ---
bool manterConexoes() {
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - ultimaTentativaWiFi >= INTERVALO_RECONEXAO) {
            ultimaTentativaWiFi = millis();
            Serial.printf("Wi-Fi desconectado. Reconectando a %s...\n", ssid);
            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }
        return false;
    }

    if (!mqtt.connected()) {
        if (millis() - ultimaTentativaMqtt >= INTERVALO_RECONEXAO) {
            ultimaTentativaMqtt = millis();
            String clientId = "SAPE_Apresentacao_" + String((uint32_t)esp_random(), HEX);
            Serial.print("Conectando ao MQTT...");
            if (mqtt.connect(clientId.c_str())) {
                Serial.println(" OK!");
            } else {
                Serial.printf(" falha (código %d). Nova tentativa em %lus.\n",
                              mqtt.state(), INTERVALO_RECONEXAO / 1000);
            }
        }
        return mqtt.connected();
    }

    return true;
}
