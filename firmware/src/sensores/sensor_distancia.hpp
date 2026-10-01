#pragma once

#include <optional>

class SensorDistancia {
 public:
  virtual ~SensorDistancia() = default;

  virtual void iniciar() = 0;
  [[nodiscard]] virtual std::optional<int> lerMilimetros() = 0;
};
