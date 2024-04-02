cmake_minimum_required(VERSION 3.16)
idf_component_register(SRCS arduinoFFT/src/arduinoFFT.cpp INCLUDE_DIRS arduinoFFT/src REQUIRES arduino)
project(arduinoFFT)
