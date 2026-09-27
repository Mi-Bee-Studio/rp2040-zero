# 标准 pico_sdk_import.cmake（pico-examples 同款，精简注释版）：
# 前置设 PICO_SDK_PATH 指向本地 SDK；或 PICO_SDK_FETCH_FROM_GIT=1 自动克隆。
# 注意：stdio_usb 依赖 SDK 的 lib/tinyusb 子模块（克隆 SDK 后
# `git -C $PICO_SDK_PATH submodule update --init lib/tinyusb`）。

if (DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    message("Using PICO_SDK_PATH from environment ('${PICO_SDK_PATH}')")
endif ()

set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")

if (PICO_SDK_FETCH_FROM_GIT AND (NOT EXISTS ${PICO_SDK_PATH}))
    include(FetchContent)
    set(FETCHCONTENT_BASE_DIR_SAVE ${FETCHCONTENT_BASE_DIR})
    if (PICO_SDK_FETCH_FROM_GIT_PATH)
        get_filename_component(FETCHCONTENT_BASE_DIR
            "${PICO_SDK_FETCH_FROM_GIT_PATH}" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")
    endif ()
    FetchContent_Declare(
            pico_sdk
            GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk
            GIT_TAG ${PICO_SDK_FETCH_FROM_GIT_TAG}
    )
    if (NOT pico_sdk)
        message("Downloading Raspberry Pi Pico SDK")
        FetchContent_Populate(pico_sdk)
        set(PICO_SDK_PATH ${CMAKE_BINARY_DIR}/_deps/pico_sdk-src)
    endif ()
    set(FETCHCONTENT_BASE_DIR ${FETCHCONTENT_BASE_DIR_SAVE})
endif ()

if (NOT EXISTS ${PICO_SDK_PATH})
    message(FATAL_ERROR "Directory '${PICO_SDK_PATH}' not found")
endif ()

set(PICO_SDK_INIT_CMAKE_FILE ${PICO_SDK_PATH}/pico_sdk_init.cmake)
include(${PICO_SDK_INIT_CMAKE_FILE})
