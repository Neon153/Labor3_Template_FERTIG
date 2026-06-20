cmake_minimum_required(VERSION 3.25)

## For proper debugging, You may need to set this manually
# set(CMAKE_BUILD_TYPE "Debug")

include(${CMAKE_CURRENT_LIST_DIR}/../config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../external/pico-sdk/pico_sdk_init.cmake)

project(lab3 LANGUAGES C CXX)

pico_sdk_init()

add_subdirectory(src)
