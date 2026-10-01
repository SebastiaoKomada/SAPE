#include "energia/politica_sono.hpp"

#include "config.hpp"

using namespace std;

namespace {

bool tempoLimpo(const DadosPonto& ponto) {
  return ponto.valido &&
         ponto.chuva_atual_mm <= config::kChuvaDesprezivelMm &&
         ponto.chuva_proximas_6h_mm <= config::kChuvaDesprezivelMm &&
         ponto.prob_chuva_max_pct <= config::kProbabilidadeChuvaMaximaEconomia &&
         ponto.weather_code <= config::kWeatherCodeMaximoLimpo;
}

bool chuvaForteAMontante(const DadosPonto& ponto) {
  return ponto.valido &&
         (ponto.chuva_ultimas_6h_mm >= config::kLimiarChuvaMontante6h ||
          ponto.chuva_proximas_6h_mm >= config::kLimiarPrevisaoMontante6h);
}

}  // namespace

const char* nomeModo(Modo modo) {
  switch (modo) {
    case Modo::kAlerta:
      return "alert";
    case Modo::kEconomia:
      return "economy";
    case Modo::kNormal:
      break;
  }
  return "normal";
}

Decisao decidirSono(const ResultadoClima& clima, optional<int> subida_mm) {
  // O nivel medido e a evidencia mais direta: vale mesmo sem dados de clima.
  if (subida_mm && *subida_mm >= config::kLimiarSubidaMm) {
    return {Modo::kAlerta, config::kSonoAlerta};
  }

  if (chuvaForteAMontante(clima.montante)) {
    return {Modo::kAlerta, config::kSonoAlerta};
  }

  // Economia exige os dois pontos validos e limpos: se a API falhou, nunca
  // se estica o sono.
  if (tempoLimpo(clima.montante) && tempoLimpo(clima.local)) {
    return {Modo::kEconomia, config::kSonoEconomia};
  }

  return {Modo::kNormal, config::kSonoNormal};
}
