#pragma once

#include <chrono>

#include "driver/gpio.h"

namespace config {

inline constexpr char kWifiNome[] = "WLL-Inatel";
inline constexpr char kWifiSenha[] = "inatelsemfio";
inline constexpr char kMqttEndereco[] = "mqtt://SEU_BROKER:1883";
inline constexpr char kMqttUsuario[] = "";
inline constexpr char kMqttSenha[] = "";
inline constexpr char kIdDispositivo[] = "a02yyuw-001";
inline constexpr char kTopicoMqtt[] =
    "sape/sensors/a02yyuw-001/telemetry";

inline constexpr bool kSimularSensor = true;
inline constexpr gpio_num_t kPinoRxSensor = GPIO_NUM_33;

// Com kUsarDeepSleep=false o ciclo se repete a cada kIntervaloEnvio sem
// desligar o Wi-Fi; util em bancada, onde o deep sleep derruba a serial.
inline constexpr bool kUsarDeepSleep = true;
inline constexpr auto kIntervaloEnvio = std::chrono::seconds{5};

// Leitura do sensor ao acordar: mediana de kAmostrasSensor leituras validas.
inline constexpr int kAmostrasSensor = 5;
inline constexpr auto kTimeoutSensor = std::chrono::seconds{3};

// Clima (Open-Meteo). SUBSTITUIR pelas coordenadas reais.
// Montante: ponto Z, antes da area X no mesmo rio.
// Local: area X onde o dispositivo esta instalado.
inline constexpr char kUrlClima[] = "https://api.open-meteo.com/v1/forecast";
inline constexpr double kLatitudeMontante = -22.3000;
inline constexpr double kLongitudeMontante = -45.6500;
inline constexpr double kLatitudeLocal = -22.2500;
inline constexpr double kLongitudeLocal = -45.7000;

// Limiares da politica de sono.
inline constexpr double kLimiarChuvaMontante6h = 10.0;      // mm
inline constexpr double kLimiarPrevisaoMontante6h = 15.0;   // mm
inline constexpr int kLimiarSubidaMm = 100;                 // entre ciclos
inline constexpr double kChuvaDesprezivelMm = 0.1;
inline constexpr int kProbabilidadeChuvaMaximaEconomia = 20;  // %
inline constexpr int kWeatherCodeMaximoLimpo = 3;  // 0-3: limpo ou nublado

// Duracao do deep sleep por modo.
inline constexpr auto kSonoAlerta = std::chrono::minutes{5};
inline constexpr auto kSonoNormal = std::chrono::minutes{30};
inline constexpr auto kSonoEconomia = std::chrono::hours{2};

// Tempo maximo de espera de cada etapa, para nunca prender o ESP acordado.
inline constexpr auto kTimeoutWifi = std::chrono::seconds{20};
inline constexpr auto kTimeoutMqtt = std::chrono::seconds{10};
inline constexpr auto kTimeoutHttp = std::chrono::seconds{8};
inline constexpr auto kTimeoutPublicacao = std::chrono::seconds{5};

}  // namespace config
