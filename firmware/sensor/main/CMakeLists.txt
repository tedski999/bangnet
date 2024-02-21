idf_component_register(SRCS main.cpp ntp.cpp oled.cpp ota.cpp INCLUDE_DIRS "") # EMBED_TXTFILES ca_cert.pem)
# REQUIRES app_update
# REQUIRES esp_http_client
