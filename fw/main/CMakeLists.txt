idf_component_register(
  SRCS main.c wifi_conn.c ../../misc/net/net_tsinghua.c time_sync.c request.c i2s.c audio.c led.c
  INCLUDE_DIRS ""
  EMBED_TXTFILES ca.pem
)

set(ulp_app_name ulp_duck)
set(ulp_sources "ulp/main.c")
set(ulp_exp_dep_srcs "main.c")
ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")
