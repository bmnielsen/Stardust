cmake_minimum_required(VERSION 3.24)

add_library(BWAPICore OBJECT
  APMCounter.cpp
  FPSCounter.cpp)

set_property(TARGET BWAPICore PROPERTY POSITION_INDEPENDENT_CODE ON)
