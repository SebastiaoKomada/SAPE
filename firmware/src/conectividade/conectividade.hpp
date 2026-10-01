#pragma once

#include <atomic>
#include <cstdint>

#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"

class Conectividade {
 public:
  void iniciar();
  bool aguardarWifi(TickType_t espera) const;
  bool aguardarMqtt(TickType_t espera) const;
  bool publicar(const char* topico, const char* mensagem) const;
  // Espera o PUBACK da ultima publicacao; sem isso o deep sleep pode
  // derrubar a mensagem antes de ela sair.
  bool aguardarPublicacao(TickType_t espera) const;
  // Encerra MQTT e Wi-Fi antes do deep sleep.
  void desligar();

 private:
  static void eventoWifi(void* contexto, esp_event_base_t base, int32_t id,
                         void* dados);
  static void eventoMqtt(void* contexto, esp_event_base_t base, int32_t id,
                         void* dados);

  void tratarEventoWifi(esp_event_base_t base, int32_t id, void* dados);
  void tratarEventoMqtt(esp_event_base_t base, int32_t id, void* dados);
  void iniciarMqtt();

  EventGroupHandle_t eventos_{};
  esp_mqtt_client_handle_t cliente_mqtt_{};
  bool mqtt_iniciado_{};
  std::atomic<bool> desligando_{};
};
