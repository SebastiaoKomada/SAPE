#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "driver/uart.h"
#include "sensores/sensor_distancia.hpp"

class SensorA02yyuw final : public SensorDistancia {
 public:
  void iniciar() override;
  [[nodiscard]] std::optional<int> lerMilimetros() override;

 private:
  static constexpr uart_port_t kPortaUart = UART_NUM_2;
  static constexpr uint8_t kInicioQuadro = 0xFF;

  std::array<uint8_t, 4> quadro_{};
  std::size_t indice_{};
};
