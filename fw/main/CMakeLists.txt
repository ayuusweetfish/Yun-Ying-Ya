idf_component_register(
  SRCS main.c wifi_conn.c ../../misc/net/net_tsinghua.c time_sync.c request.c i2s.c audio.c led.c gauge.c
  INCLUDE_DIRS ""
  EMBED_TXTFILES ca.pem
  REQUIRES
    esp-tls
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_ledc
    esp_http_client
    esp_timer
    esp_wifi
    nvs_flash
    ulp
)

set(ulp_app_name ulp_duck)
set(ulp_sources "ulp/main.c")
set(ulp_exp_dep_srcs "main.c")
ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")
