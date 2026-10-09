from datetime import UTC, datetime, timedelta

import pytest

from app.analysis import choose_bucket_seconds, rise_rate_cm_per_h

T0 = datetime(2026, 10, 9, 12, 0, tzinfo=UTC)


def test_rise_rate_linear():
    # sobe 0,10 m a cada 15 min = 40 cm/h
    points = [(T0 + timedelta(minutes=15 * i), 1.0 + 0.10 * i) for i in range(5)]
    assert rise_rate_cm_per_h(points) == pytest.approx(40.0)


def test_rise_rate_falling():
    points = [(T0, 2.0), (T0 + timedelta(hours=1), 1.5)]
    assert rise_rate_cm_per_h(points) == pytest.approx(-50.0)


def test_rise_rate_smooths_noise():
    # ruído alternado em torno de um nível constante não deve virar tendência
    points = [(T0 + timedelta(minutes=15 * i), 1.0 + (0.02 if i % 2 else -0.02)) for i in range(4)]
    assert abs(rise_rate_cm_per_h(points)) < 5


def test_rise_rate_needs_two_points():
    assert rise_rate_cm_per_h([]) is None
    assert rise_rate_cm_per_h([(T0, 1.0)]) is None
    assert rise_rate_cm_per_h([(T0, 1.0), (T0, 1.2)]) is None


def test_bucket_raw_when_few_points():
    # 15 min de intervalo por 7 dias = 672 pontos -> agrega; 24 h = 96 -> bruto
    assert choose_bucket_seconds(span_s=86400, count=96) is None
    assert choose_bucket_seconds(span_s=3600, count=300) is None


def test_bucket_keeps_chart_under_limit():
    for span_s, count in [(3600, 720), (86400, 17280), (7 * 86400, 672), (30 * 86400, 518400)]:
        bucket = choose_bucket_seconds(span_s=span_s, count=count)
        assert bucket is not None
        assert span_s / bucket <= 300


def test_bucket_picks_smallest_round_interval():
    # 1 h a cada 5 s: precisa de >= 12 s -> 30 s
    assert choose_bucket_seconds(span_s=3600, count=720) == 30


def test_bucket_beyond_one_day():
    assert choose_bucket_seconds(span_s=400 * 86400, count=10_000) == 2 * 86400
