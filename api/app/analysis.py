"""Cálculos sobre as medições (funções puras, sem I/O)."""

import math
from collections.abc import Sequence
from datetime import datetime

MAX_CHART_POINTS = 300

# Intervalos "redondos" de agregação, em segundos
_BUCKETS_S = (10, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 10800, 21600, 43200, 86400)


def rise_rate_cm_per_h(points: Sequence[tuple[datetime, float]]) -> float | None:
    """Inclinação da regressão linear do nível (m) no tempo, em cm/h.

    Precisa de ao menos 2 pontos em instantes diferentes.
    """
    if len(points) < 2:
        return None
    t0 = points[0][0]
    xs = [(t - t0).total_seconds() for t, _ in points]
    ys = [level for _, level in points]
    mean_x = sum(xs) / len(xs)
    mean_y = sum(ys) / len(ys)
    var_x = sum((x - mean_x) ** 2 for x in xs)
    if var_x == 0:
        return None
    slope_m_per_s = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys)) / var_x
    return slope_m_per_s * 100 * 3600


def choose_bucket_seconds(
    span_s: float, count: int, max_points: int = MAX_CHART_POINTS
) -> int | None:
    """Intervalo de agregação para o gráfico, ou None para devolver os dados brutos."""
    if count <= max_points:
        return None
    needed = span_s / max_points
    for bucket in _BUCKETS_S:
        if bucket >= needed:
            return bucket
    return math.ceil(needed / 86400) * 86400
