cmake_minimum_required(VERSION 3.16)
idf_component_register(
	SRCS ArduinoHttpClient/src/b64.cpp ArduinoHttpClient/src/HttpClient.cpp ArduinoHttpClient/src/URLEncoder.cpp ArduinoHttpClient/src/WebSocketClient.cpp
	INCLUDE_DIRS ArduinoHttpClient/src
	REQUIRES arduino)
project(ArduinoHttpClient)
