#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "pico.h"

/* 光照 —— 模拟输出接 ADC 脚。模块标称 TEMT6000、实为 CdS 光敏电阻
 * （两点定标实锤，见 light.c）；换算为幂律曲线，参数可经串口 luxcal
 * 调整并持久化（cfgstore）。mv 返回原始毫伏（诊断用，可 NULL）。 */
#define LIGHT_CAL_A_DEFAULT     39.4f   /* lux = A × (v/(Vref−v))^GAMMA */
#define LIGHT_CAL_GAMMA_DEFAULT 1.615f

void light_init(uint adc_pin);
void light_set_cal(float a, float gamma); /* 运行时改曲线参数（cfgstore 调） */
bool light_read(float *lux, uint32_t *mv);
