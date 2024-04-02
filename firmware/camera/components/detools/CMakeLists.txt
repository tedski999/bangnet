cmake_minimum_required(VERSION 3.16)
idf_component_register(
	SRCS detools/c/detools.c detools/c/heatshrink/heatshrink_decoder.c
	INCLUDE_DIRS detools/c detools/c/heatshrink)
project(detools)
