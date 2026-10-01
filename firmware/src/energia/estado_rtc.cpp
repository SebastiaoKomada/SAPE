#include "energia/estado_rtc.hpp"

#include "esp_attr.h"

namespace {

RTC_DATA_ATTR uint32_t boots = 0;
RTC_DATA_ATTR int ultima_distancia_mm = -1;

}  // namespace

namespace estado_rtc {

uint32_t registrarBoot() { return ++boots; }

std::optional<int> registrarDistancia(int distancia_mm) {
  std::optional<int> subida;
  if (ultima_distancia_mm >= 0) {
    subida = ultima_distancia_mm - distancia_mm;
  }
  ultima_distancia_mm = distancia_mm;
  return subida;
}

}  // namespace estado_rtc
