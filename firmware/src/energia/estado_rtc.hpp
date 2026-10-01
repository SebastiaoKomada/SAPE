#pragma once

#include <cstdint>
#include <optional>

// Estado que sobrevive ao deep sleep (memoria RTC). Zera em reset de energia.
namespace estado_rtc {

// Incrementa e devolve o numero de boots desde a ultima falta de energia.
uint32_t registrarBoot();

// Quanto o nivel subiu desde a leitura anterior (anterior - atual, em mm);
// nullopt se nao houver leitura anterior. Atualiza a leitura guardada.
std::optional<int> registrarDistancia(int distancia_mm);

}  // namespace estado_rtc
