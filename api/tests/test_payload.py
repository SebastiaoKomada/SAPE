import pytest

from app.ingest.payload import InvalidPayload, parse_payload, parse_topic, river_level


@pytest.mark.parametrize(
    ("topic", "expected"),
    [
        ("sape/estacao-01/telemetria", "estacao-01"),
        ("sape//telemetria", None),
        ("sape/estacao-01/outra", None),
        ("inatel/sape/teste_apresentacao", None),
        ("sape/a/b/telemetria", None),
    ],
)
def test_parse_topic(topic, expected):
    assert parse_topic(topic) == expected


def test_valid_reading():
    r = parse_payload(b'{"distancia_m": 1.234, "sensor_ok": true, "bateria_v": 4.01, "solar_v": 5.2}')
    assert r.distance_m == 1.234
    assert r.sensor_ok is True
    assert r.battery_v == 4.01
    assert r.solar_v == 5.2


def test_sensor_failure_keeps_telemetry():
    r = parse_payload(b'{"distancia_m": null, "sensor_ok": false, "bateria_v": 3.9, "solar_v": 0.1}')
    assert r.distance_m is None
    assert r.sensor_ok is False
    assert r.battery_v == 3.9


@pytest.mark.parametrize("distance", [0.0, 0.02, 4.51, 12.0])
def test_out_of_range_distance_becomes_failure(distance):
    r = parse_payload(f'{{"distancia_m": {distance}, "sensor_ok": true, "bateria_v": 4, "solar_v": 5}}')
    assert r.distance_m is None
    assert r.sensor_ok is False


def test_ok_flag_with_null_distance_becomes_failure():
    r = parse_payload(b'{"distancia_m": null, "sensor_ok": true, "bateria_v": 4, "solar_v": 5}')
    assert r.sensor_ok is False


def test_failure_flag_discards_distance():
    r = parse_payload(b'{"distancia_m": 1.5, "sensor_ok": false, "bateria_v": 4, "solar_v": 5}')
    assert r.distance_m is None


@pytest.mark.parametrize(
    "raw",
    [b"not json", b"{}", b'{"distancia_m": 1.0, "sensor_ok": true, "bateria_v": 4}', b"[]"],
)
def test_invalid_payload(raw):
    with pytest.raises(InvalidPayload):
        parse_payload(raw)


def test_river_level():
    assert river_level(3.0, 1.25) == pytest.approx(1.75)
    assert river_level(None, 1.25) is None
    assert river_level(3.0, None) is None
