#pragma once

#include <cstdint>
#include <optional>

#include "clima/dados_clima.hpp"
#include "energia/politica_sono.hpp"

class Conectividade;

class PublicadorTelemetria {
 public:
  explicit PublicadorTelemetria(const Conectividade& conectividade);

  // distancia_mm vazio indica que o sensor nao entregou leitura valida.
  void publicar(std::optional<int> distancia_mm, const ResultadoClima& clima,
                const Decisao& decisao, uint32_t boots) const;

 private:
  const Conectividade& conectividade_;
};
