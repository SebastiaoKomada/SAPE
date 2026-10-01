#pragma once

#include <chrono>
#include <optional>

#include "clima/dados_clima.hpp"

enum class Modo { kAlerta, kNormal, kEconomia };

struct Decisao {
  Modo modo;
  std::chrono::seconds sono;
};

const char* nomeModo(Modo modo);

// subida_mm: quanto o nivel subiu desde a ultima leitura (distancia anterior
// menos a atual); nullopt quando nao ha leitura anterior ou atual.
Decisao decidirSono(const ResultadoClima& clima,
                    std::optional<int> subida_mm);
