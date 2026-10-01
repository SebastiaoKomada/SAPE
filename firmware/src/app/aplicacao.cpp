#include "app/aplicacao.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <optional>

#include "clima/cliente_clima.hpp"
#include "config.hpp"
#include "conectividade/conectividade.hpp"
#include "energia/estado_rtc.hpp"
#include "energia/politica_sono.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sensores/sensor_a02yyuw.hpp"
#include "sensores/sensor_distancia.hpp"
#include "telemetria/publicador_telemetria.hpp"

using namespace std;

namespace {

constexpr char kTag[] = "sensor-mqtt";

template <typename Duracao>
TickType_t emTicks(Duracao duracao) {
  return pdMS_TO_TICKS(
      chrono::duration_cast<chrono::milliseconds>(duracao).count());
}

void iniciarNvs() {
  esp_err_t resultado = nvs_flash_init();
  if (resultado == ESP_ERR_NVS_NO_FREE_PAGES ||
      resultado == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    resultado = nvs_flash_init();
  }
  ESP_ERROR_CHECK(resultado);
}

unique_ptr<SensorDistancia> criarSensor() {
  return make_unique<SensorA02yyuw>();
}

// Logo apos o boot o sensor ainda esta aquecendo e lerMilimetros() devolve
// nullopt; por isso insiste ate juntar as amostras ou estourar o tempo, e usa
// a mediana para descartar leituras espurias.
optional<int> medirDistancia(SensorDistancia& sensor) {
  array<int, config::kAmostrasSensor> amostras{};
  size_t total = 0;
  const TickType_t limite =
      xTaskGetTickCount() + emTicks(config::kTimeoutSensor);

  while (total < amostras.size() && xTaskGetTickCount() < limite) {
    if (const auto mm = sensor.lerMilimetros()) {
      amostras[total++] = *mm;
    }
    // O A02YYUW emite um quadro a cada ~100 ms.
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (total == 0) {
    return nullopt;
  }
  sort(amostras.begin(), amostras.begin() + total);
  return amostras[total / 2];
}

[[noreturn]] void dormir(chrono::seconds duracao) {
  ESP_LOGI(kTag, "Deep sleep por %lld s",
           static_cast<long long>(duracao.count()));
  ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(
      chrono::duration_cast<chrono::microseconds>(duracao).count()));
  esp_deep_sleep_start();
}

// Um ciclo completo: medir, consultar o clima, decidir o sono e publicar.
// Cada espera tem timeout; se a rede falhar o ciclo termina sem publicar,
// mas a decisao de sono continua valendo.
Decisao executarCiclo(SensorDistancia& sensor, const Conectividade& rede,
                      const ClienteClima& clima_api,
                      const PublicadorTelemetria& publicador) {
  const uint32_t boots = estado_rtc::registrarBoot();

  // O Wi-Fi ja esta conectando em paralelo enquanto o sensor aquece.
  const auto distancia = medirDistancia(sensor);
  if (distancia) {
    ESP_LOGI(kTag, "Distancia: %d mm", *distancia);
  } else {
    ESP_LOGW(kTag, "Sem leitura valida do A02YYUW");
  }
  const auto subida = distancia ? estado_rtc::registrarDistancia(*distancia)
                                : nullopt;

  ResultadoClima clima;
  if (rede.aguardarWifi(emTicks(config::kTimeoutWifi))) {
    clima = clima_api.buscar();
  } else {
    ESP_LOGW(kTag, "Wi-Fi indisponivel; seguindo sem dados de clima");
  }

  const Decisao decisao = decidirSono(clima, subida);
  ESP_LOGI(kTag, "Modo %s, sono de %lld s", nomeModo(decisao.modo),
           static_cast<long long>(decisao.sono.count()));

  if (!rede.aguardarMqtt(emTicks(config::kTimeoutMqtt))) {
    ESP_LOGW(kTag, "MQTT indisponivel; telemetria descartada neste ciclo");
    return decisao;
  }

  publicador.publicar(distancia, clima, decisao, boots);
  if (!rede.aguardarPublicacao(emTicks(config::kTimeoutPublicacao))) {
    ESP_LOGW(kTag, "Sem confirmacao de publicacao do broker");
  }
  return decisao;
}

}  // namespace

void Aplicacao::executar() {
  iniciarNvs();

  auto sensor = criarSensor();
  sensor->iniciar();

  Conectividade conectividade;
  conectividade.iniciar();
  PublicadorTelemetria publicador{conectividade};
  const ClienteClima clima{};

  ESP_LOGI(kTag, "Modo do sensor: %s",
           config::kSimularSensor ? "simulado" : "A02YYUW");

  while (true) {
    const Decisao decisao =
        executarCiclo(*sensor, conectividade, clima, publicador);

    if (config::kUsarDeepSleep) {
      conectividade.desligar();
      dormir(decisao.sono);
    }

    vTaskDelay(emTicks(config::kIntervaloEnvio));
  }
}
