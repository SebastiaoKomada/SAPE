#include "telemetria/publicador_telemetria.hpp"

#include <array>
#include <memory>

#include "cJSON.h"
#include "config.hpp"
#include "conectividade/conectividade.hpp"
#include "esp_log.h"
#include "esp_timer.h"

using namespace std;

namespace {

constexpr char kTag[] = "sensor-mqtt";

struct DestruirJson {
  void operator()(cJSON* json) const { cJSON_Delete(json); }
};

bool adicionarDistancia(cJSON* json, optional<int> distancia_mm) {
  if (!cJSON_AddBoolToObject(json, "sensor_ok", distancia_mm.has_value())) {
    return false;
  }
  if (!distancia_mm) {
    return true;
  }
  return cJSON_AddNumberToObject(json, "distance_mm", *distancia_mm) &&
         cJSON_AddNumberToObject(json, "distance_cm", *distancia_mm / 10.0);
}

bool adicionarClima(cJSON* json, const ResultadoClima& clima) {
  const bool valido = clima.montante.valido && clima.local.valido;
  if (!cJSON_AddBoolToObject(json, "weather_valid", valido)) {
    return false;
  }
  if (clima.montante.valido &&
      !(cJSON_AddNumberToObject(json, "rain_upstream_6h_mm",
                                clima.montante.chuva_ultimas_6h_mm) &&
        cJSON_AddNumberToObject(json, "rain_upstream_forecast_mm",
                                clima.montante.chuva_proximas_6h_mm) &&
        cJSON_AddNumberToObject(json, "rain_upstream_prob_pct",
                                clima.montante.prob_chuva_max_pct))) {
    return false;
  }
  if (clima.local.valido &&
      !(cJSON_AddNumberToObject(json, "rain_local_mm",
                                clima.local.chuva_atual_mm) &&
        cJSON_AddNumberToObject(json, "weather_code_local",
                                clima.local.weather_code))) {
    return false;
  }
  return true;
}

}  // namespace

PublicadorTelemetria::PublicadorTelemetria(
    const Conectividade& conectividade)
    : conectividade_{conectividade} {}

void PublicadorTelemetria::publicar(optional<int> distancia_mm,
                                    const ResultadoClima& clima,
                                    const Decisao& decisao,
                                    uint32_t boots) const {
  unique_ptr<cJSON, DestruirJson> json{cJSON_CreateObject()};
  if (!json ||
      !cJSON_AddStringToObject(json.get(), "device_id",
                               config::kIdDispositivo) ||
      !adicionarDistancia(json.get(), distancia_mm) ||
      !adicionarClima(json.get(), clima) ||
      !cJSON_AddStringToObject(json.get(), "mode", nomeModo(decisao.modo)) ||
      !cJSON_AddNumberToObject(json.get(), "sleep_s",
                               static_cast<double>(decisao.sono.count())) ||
      !cJSON_AddNumberToObject(json.get(), "boot_count", boots) ||
      !cJSON_AddNumberToObject(json.get(), "uptime_ms",
                               esp_timer_get_time() / 1000.0)) {
    ESP_LOGE(kTag, "Sem memoria para montar a telemetria");
    return;
  }

  array<char, 512> mensagem{};
  if (!cJSON_PrintPreallocated(json.get(), mensagem.data(), mensagem.size(),
                              false)) {
    ESP_LOGE(kTag, "Mensagem MQTT excede o tamanho permitido");
    return;
  }

  conectividade_.publicar(config::kTopicoMqtt, mensagem.data());
}
