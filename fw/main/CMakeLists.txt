idf_component_register(
  SRCS main.c wifi_conn.c ../../misc/net/net_tsinghua.c time_sync.c request.c i2s.c audio.c encode.c led.c gauge.c
  INCLUDE_DIRS "" "opus-1.5.2/include"
  EMBED_TXTFILES ca.pem
  REQUIRES
    esp-tls
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_ledc
    esp_driver_mcpwm
    esp_http_client
    esp_timer
    esp_wifi
    nvs_flash
    spi_flash
    ulp
)
add_prebuilt_library(libopus "opus-1.5.2/build/.libs/libopus.a")

set(ulp_app_name ulp_duck)
set(ulp_sources "ulp/main.c" "ulp/sled.S")
set(ulp_exp_dep_srcs "main.c")
ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")
