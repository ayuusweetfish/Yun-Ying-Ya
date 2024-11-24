idf_component_register(
  SRCS main.c wifi_conn.c ../../misc/net/net_tsinghua.c time_sync.c request.c i2s.c audio.c
  INCLUDE_DIRS ""
  EMBED_TXTFILES ca.pem
)
