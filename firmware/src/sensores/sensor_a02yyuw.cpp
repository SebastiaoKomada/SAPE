#include "sensores/sensor_a02yyuw.hpp"

#include "config.hpp"
#include "esp_err.h"
#include "esp_log.h"

using namespace std;

namespace {

constexpr char kTag[] = "sensor-mqtt";

}  // namespace

void SensorA02yyuw::iniciar() {
  uart_config_t uart_config{};
  uart_config.baud_rate = 9600;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_DEFAULT;

  ESP_ERROR_CHECK(uart_param_config(kPortaUart, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(kPortaUart, UART_PIN_NO_CHANGE,
                               config::kPinoRxSensor, UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(kPortaUart, 1024, 0, 0, nullptr, 0));
}

optional<int> SensorA02yyuw::lerMilimetros() {
  uint8_t byte{};

  while (uart_read_bytes(kPortaUart, &byte, 1, 0) == 1) {
    if (indice_ == 0) {
      if (byte == kInicioQuadro) {
        quadro_[indice_++] = byte;
      }
      continue;
    }

    quadro_[indice_++] = byte;
    if (indice_ != quadro_.size()) {
      continue;
    }

    indice_ = 0;
    const auto checksum =
        static_cast<uint8_t>(quadro_[0] + quadro_[1] + quadro_[2]);
    if (checksum != quadro_[3]) {
      ESP_LOGW(kTag, "Quadro A02YYUW invalido");
      continue;
    }

    return (static_cast<int>(quadro_[1]) << 8) | quadro_[2];
  }

  return nullopt;
}
