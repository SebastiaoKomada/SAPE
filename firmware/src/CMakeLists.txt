set(SOURCES
    "main.cpp"
    "app/aplicacao.cpp"
    "clima/cliente_clima.cpp"
    "conectividade/conectividade.cpp"
    "energia/estado_rtc.cpp"
    "energia/politica_sono.cpp"
    "sensores/sensor_a02yyuw.cpp"
    "telemetria/publicador_telemetria.cpp"
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_driver_uart
        esp-tls
        esp_event
        esp_hw_support
        esp_http_client
        esp_netif
        esp_timer
        esp_wifi
        json
        mqtt
        nvs_flash
)

set_source_files_properties(${SOURCES} PROPERTIES COMPILE_OPTIONS "-std=c++17")
