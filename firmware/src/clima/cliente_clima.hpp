#pragma once

#include "clima/dados_clima.hpp"

class ClienteClima {
 public:
  // Consulta a Open-Meteo para o ponto a montante e o local. Nunca falha em
  // voz alta: se algo der errado, os pontos voltam com valido=false.
  ResultadoClima buscar() const;
};
