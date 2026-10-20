cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Dynamic VERSION 1.0.0 LANGUAGES CXX)

if(NOT DEFINED GEODE_SDK_PATH)
    message(FATAL_ERROR "GEODE_SDK_PATH is not set. Set it to the path of the Geode SDK.")
endif()

include(${GEODE_SDK_PATH}/cmake/geode.cmake)

geode_add_mod(Dynamic
    SOURCES
        src/main.cpp
        src/DynamicMenu.cpp
        src/DynamicLogic.cpp
)