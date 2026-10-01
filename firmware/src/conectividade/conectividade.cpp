#include "conectividade/conectividade.hpp"

#include <algorithm>

#include "config.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

using namespace std;

namespace {

constexpr char kTag[] = "sensor-mqtt";
constexpr EventBits_t kMqttConectado = BIT0;
constexpr EventBits_t kWifiConectado = BIT1;
constexpr EventBits_t kMqttPublicado = BIT2;

}  // namespace

void Conectividade::iniciar() {
  eventos_ = xEventGroupCreate();
  ESP_ERROR_CHECK(eventos_ == nullptr ? ESP_ERR_NO_MEM : ESP_OK);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta() == nullptr
                      ? ESP_ERR_NO_MEM
                      : ESP_OK);

  wifi_init_config_t config_inicial = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&config_inicial));
  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &Conectividade::eventoWifi, this));
  ESP_ERROR_CHECK(esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &Conectividade::eventoWifi, this));

  wifi_config_t config_wifi{};
  static_assert(sizeof(config::kWifiNome) <= sizeof(config_wifi.sta.ssid),
                "O nome do Wi-Fi excede o limite do ESP32");
  static_assert(sizeof(config::kWifiSenha) <=
                    sizeof(config_wifi.sta.password),
                "A senha do Wi-Fi excede o limite do ESP32");
  copy_n(config::kWifiNome, sizeof(config::kWifiNome), config_wifi.sta.ssid);
  copy_n(config::kWifiSenha, sizeof(config::kWifiSenha),
         config_wifi.sta.password);
  config_wifi.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config_wifi));
  ESP_ERROR_CHECK(esp_wifi_start());
}

bool Conectividade::aguardarWifi(TickType_t espera) const {
  return (xEventGroupWaitBits(eventos_, kWifiConectado, pdFALSE, pdTRUE,
                              espera) &
          kWifiConectado) != 0;
}

bool Conectividade::aguardarMqtt(TickType_t espera) const {
  return (xEventGroupWaitBits(eventos_, kMqttConectado, pdFALSE, pdTRUE,
                              espera) &
          kMqttConectado) != 0;
}

bool Conectividade::aguardarPublicacao(TickType_t espera) const {
  return (xEventGroupWaitBits(eventos_, kMqttPublicado, pdFALSE, pdTRUE,
                              espera) &
          kMqttPublicado) != 0;
}

void Conectividade::desligar() {
  // Impede que o handler reconecte o Wi-Fi que estamos derrubando.
  desligando_ = true;
  if (cliente_mqtt_ != nullptr) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mqtt_client_stop(cliente_mqtt_));
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
}

bool Conectividade::publicar(const char* topico, const char* mensagem) const {
  xEventGroupClearBits(eventos_, kMqttPublicado);
  const int id_mensagem =
      esp_mqtt_client_publish(cliente_mqtt_, topico, mensagem, 0, 1, 0);
  if (id_mensagem < 0) {
    ESP_LOGE(kTag, "Falha ao publicar mensagem MQTT");
    return false;
  }

  ESP_LOGI(kTag, "Publicado (id=%d): %s", id_mensagem, mensagem);
  return true;
}

void Conectividade::eventoWifi(void* contexto, esp_event_base_t base,
                               int32_t id, void* dados) {
  static_cast<Conectividade*>(contexto)->tratarEventoWifi(base, id, dados);
}

void Conectividade::eventoMqtt(void* contexto, esp_event_base_t base,
                               int32_t id, void* dados) {
  static_cast<Conectividade*>(contexto)->tratarEventoMqtt(base, id, dados);
}

void Conectividade::tratarEventoWifi(esp_event_base_t base, int32_t id,
                                     [[maybe_unused]] void* dados) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    ESP_ERROR_CHECK(esp_wifi_connect());
    return;
  }

  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(eventos_, kMqttConectado | kWifiConectado);
    if (desligando_) {
      return;
    }
    ESP_LOGW(kTag, "Wi-Fi desconectado; tentando reconectar");
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
    return;
  }

  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(kTag, "Wi-Fi conectado");
    xEventGroupSetBits(eventos_, kWifiConectado);
    iniciarMqtt();
  }
}

void Conectividade::tratarEventoMqtt(
    [[maybe_unused]] esp_event_base_t base, int32_t id,
    [[maybe_unused]] void* dados) {
  if (id == MQTT_EVENT_CONNECTED) {
    ESP_LOGI(kTag, "Conectado ao MQTT");
    xEventGroupSetBits(eventos_, kMqttConectado);
  } else if (id == MQTT_EVENT_DISCONNECTED) {
    ESP_LOGW(kTag, "MQTT desconectado");
    xEventGroupClearBits(eventos_, kMqttConectado);
  } else if (id == MQTT_EVENT_PUBLISHED) {
    xEventGroupSetBits(eventos_, kMqttPublicado);
  }
}

void Conectividade::iniciarMqtt() {
  if (mqtt_iniciado_) {
    return;
  }

  esp_mqtt_client_config_t mqtt_config{};
  mqtt_config.broker.address.uri = config::kMqttEndereco;
  mqtt_config.credentials.username = config::kMqttUsuario;
  mqtt_config.credentials.authentication.password = config::kMqttSenha;
  mqtt_config.credentials.client_id = config::kIdDispositivo;

  cliente_mqtt_ = esp_mqtt_client_init(&mqtt_config);
  ESP_ERROR_CHECK(cliente_mqtt_ == nullptr ? ESP_ERR_NO_MEM : ESP_OK);
  ESP_ERROR_CHECK(esp_mqtt_client_register_event(
      cliente_mqtt_, MQTT_EVENT_ANY, &Conectividade::eventoMqtt, this));
  ESP_ERROR_CHECK(esp_mqtt_client_start(cliente_mqtt_));
  mqtt_iniciado_ = true;
}
