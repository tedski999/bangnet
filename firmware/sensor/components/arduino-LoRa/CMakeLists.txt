cmake_minimum_required(VERSION 3.16)
idf_component_register(SRCS arduino-LoRa/src/LoRa.cpp INCLUDE_DIRS arduino-LoRa/src REQUIRES arduino)
add_compile_definitions("SSD1306_NO_SPLASH")
project(arduino-LoRa)
