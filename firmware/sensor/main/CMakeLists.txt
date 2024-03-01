idf_component_register(
	SRCS bang.cpp lora.cpp main.cpp oled.cpp ota.cpp ping.cpp sync.cpp time.cpp ../components/arduino-LoRa/src/LoRa.cpp
	INCLUDE_DIRS ../components/arduino-LoRa/src)
# EMBED_TXTFILES ca_cert.pem)
