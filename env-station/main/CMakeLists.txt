add_executable(env-station
    main.c
    cfgstore.c
    dht.c
    ssd1306.c
    light.c
    ws2812.c
)

target_link_libraries(env-station PRIVATE
    pico_stdlib
    hardware_i2c
    hardware_adc
    hardware_pio
)

# 板载 WS2812（GP16）状态灯的 PIO 程序
pico_generate_pio_header(env-station ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# 遥测/日志走 USB CDC（#ENV 行协议），不用 UART
pico_enable_stdio_usb(env-station 1)
pico_enable_stdio_uart(env-station 0)

target_compile_options(env-station PRIVATE -Wall -Wextra)

pico_add_extra_outputs(env-station) # 生成 .uf2（BOOTSEL 拖拽即刷）
