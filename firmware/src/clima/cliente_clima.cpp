#include "clima/cliente_clima.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

#include "cJSON.h"
#include "config.hpp"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

using namespace std;

namespace {

constexpr char kTag[] = "sensor-mqtt";
constexpr size_t kTamanhoCorpo = 6144;
constexpr size_t kTamanhoUrl = 384;

struct DestruirJson {
  void operator()(cJSON* json) const { cJSON_Delete(json); }
};

struct DestruirHttp {
  void operator()(esp_http_client* cliente) const {
    esp_http_client_cleanup(cliente);
  }
};

struct Corpo {
  char* dados;
  size_t tamanho;
  bool estourou;
};

esp_err_t aoEventoHttp(esp_http_client_event_t* evento) {
  if (evento->event_id != HTTP_EVENT_ON_DATA) {
    return ESP_OK;
  }

  auto* corpo = static_cast<Corpo*>(evento->user_data);
  // Reserva 1 byte para o terminador.
  if (corpo->tamanho + evento->data_len >= kTamanhoCorpo) {
    corpo->estourou = true;
    return ESP_OK;
  }
  memcpy(corpo->dados + corpo->tamanho, evento->data, evento->data_len);
  corpo->tamanho += evento->data_len;
  return ESP_OK;
}

double numeroOu(const cJSON* objeto, const char* chave, double padrao) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(objeto, chave);
  return cJSON_IsNumber(item) ? item->valuedouble : padrao;
}

// As series horarias da Open-Meteo trazem a soma da hora anterior ao
// carimbo. Comparar o texto ISO 8601 com o horario atual separa passado
// (carimbo <= agora) de futuro (carimbo > agora) sem depender de indices.
DadosPonto interpretarPonto(const cJSON* ponto) {
  DadosPonto resultado;

  const cJSON* atual = cJSON_GetObjectItemCaseSensitive(ponto, "current");
  const cJSON* horario = cJSON_GetObjectItemCaseSensitive(ponto, "hourly");
  const cJSON* hora_atual =
      cJSON_GetObjectItemCaseSensitive(atual, "time");
  const cJSON* instantes = cJSON_GetObjectItemCaseSensitive(horario, "time");
  const cJSON* chuvas =
      cJSON_GetObjectItemCaseSensitive(horario, "precipitation");
  const cJSON* probabilidades =
      cJSON_GetObjectItemCaseSensitive(horario, "precipitation_probability");

  if (!cJSON_IsObject(atual) || !cJSON_IsString(hora_atual) ||
      !cJSON_IsArray(instantes) || !cJSON_IsArray(chuvas)) {
    return resultado;
  }

  resultado.chuva_atual_mm = numeroOu(atual, "precipitation", 0.0);
  resultado.weather_code = static_cast<int>(numeroOu(atual, "weather_code", 0));

  const int total = cJSON_GetArraySize(instantes);
  for (int i = 0; i < total; ++i) {
    const cJSON* instante = cJSON_GetArrayItem(instantes, i);
    const cJSON* chuva = cJSON_GetArrayItem(chuvas, i);
    if (!cJSON_IsString(instante) || !cJSON_IsNumber(chuva)) {
      continue;
    }

    if (strcmp(instante->valuestring, hora_atual->valuestring) <= 0) {
      resultado.chuva_ultimas_6h_mm += chuva->valuedouble;
      continue;
    }

    resultado.chuva_proximas_6h_mm += chuva->valuedouble;
    const cJSON* probabilidade = cJSON_GetArrayItem(probabilidades, i);
    if (cJSON_IsNumber(probabilidade)) {
      resultado.prob_chuva_max_pct =
          max(resultado.prob_chuva_max_pct,
              static_cast<int>(probabilidade->valuedouble));
    }
  }

  resultado.valido = true;
  return resultado;
}

}  // namespace

ResultadoClima ClienteClima::buscar() const {
  ResultadoClima resultado;

  char url[kTamanhoUrl];
  const int escritos = snprintf(
      url, sizeof(url),
      "%s?latitude=%.4f,%.4f&longitude=%.4f,%.4f"
      "&current=precipitation,weather_code"
      "&hourly=precipitation,precipitation_probability"
      "&past_hours=5&forecast_hours=7&timezone=UTC",
      config::kUrlClima, config::kLatitudeMontante, config::kLatitudeLocal,
      config::kLongitudeMontante, config::kLongitudeLocal);
  if (escritos < 0 || static_cast<size_t>(escritos) >= sizeof(url)) {
    ESP_LOGE(kTag, "URL do clima excede o buffer");
    return resultado;
  }

  auto dados = make_unique<char[]>(kTamanhoCorpo);
  Corpo corpo{dados.get(), 0, false};

  esp_http_client_config_t config_http{};
  config_http.url = url;
  config_http.event_handler = aoEventoHttp;
  config_http.user_data = &corpo;
  config_http.crt_bundle_attach = esp_crt_bundle_attach;
  config_http.timeout_ms = static_cast<int>(
      chrono::duration_cast<chrono::milliseconds>(config::kTimeoutHttp)
          .count());

  unique_ptr<esp_http_client, DestruirHttp> cliente{
      esp_http_client_init(&config_http)};
  if (!cliente) {
    ESP_LOGE(kTag, "Sem memoria para o cliente HTTP");
    return resultado;
  }

  const esp_err_t erro = esp_http_client_perform(cliente.get());
  if (erro != ESP_OK) {
    ESP_LOGW(kTag, "Falha ao consultar o clima: %s", esp_err_to_name(erro));
    return resultado;
  }

  const int status = esp_http_client_get_status_code(cliente.get());
  if (status != 200 || corpo.estourou) {
    ESP_LOGW(kTag, "Resposta do clima invalida (status=%d, estourou=%d)",
             status, corpo.estourou);
    return resultado;
  }
  dados[corpo.tamanho] = '\0';

  unique_ptr<cJSON, DestruirJson> json{cJSON_Parse(dados.get())};
  if (!cJSON_IsArray(json.get()) || cJSON_GetArraySize(json.get()) < 2) {
    ESP_LOGW(kTag, "JSON do clima inesperado");
    return resultado;
  }

  resultado.montante = interpretarPonto(cJSON_GetArrayItem(json.get(), 0));
  resultado.local = interpretarPonto(cJSON_GetArrayItem(json.get(), 1));

  ESP_LOGI(kTag,
           "Clima montante: 6h=%.1f mm prev=%.1f mm prob=%d%% code=%d | "
           "local: agora=%.1f mm code=%d",
           resultado.montante.chuva_ultimas_6h_mm,
           resultado.montante.chuva_proximas_6h_mm,
           resultado.montante.prob_chuva_max_pct,
           resultado.montante.weather_code, resultado.local.chuva_atual_mm,
           resultado.local.weather_code);
  return resultado;
}
