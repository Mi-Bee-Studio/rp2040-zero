#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "pico.h"

/* 板载 WS2812 状态灯（GP16，DIN 在板上不外引）。 */
void ws2812_init(uint pin);
void ws2812_put(uint8_t r, uint8_t g, uint8_t b);
/* 250ms/拍：oled_ok && dht_ok=绿色呼吸（~2s 周期）；dht 失败=红 2Hz 闪；
 * OLED 失败=橙 1Hz 闪（屏上无从报错，灯是唯一通道） */
void ws2812_tick(bool dht_ok, bool oled_ok);
