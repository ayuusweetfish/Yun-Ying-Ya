idf_component_register(
  SRCS main.c wifi_conn.c time_sync.c request.c
  INCLUDE_DIRS ""
  EMBED_TXTFILES ca.pem
)
