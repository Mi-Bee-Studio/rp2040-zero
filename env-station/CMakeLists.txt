cmake_minimum_required(VERSION 3.13)

# 工作区约定：PICO_SDK_PATH 指向本地 SDK；或 PICO_SDK_FETCH_FROM_GIT=1 自动拉取
include(pico_sdk_import.cmake)

project(env-station C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_subdirectory(main)
