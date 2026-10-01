#pragma once

// Tipos puros, sem dependencia do IDF, para permitir teste no host.
struct DadosPonto {
  double chuva_atual_mm{};
  double chuva_ultimas_6h_mm{};
  double chuva_proximas_6h_mm{};
  int prob_chuva_max_pct{};
  int weather_code{};
  bool valido{};
};

struct ResultadoClima {
  DadosPonto montante;
  DadosPonto local;
};
