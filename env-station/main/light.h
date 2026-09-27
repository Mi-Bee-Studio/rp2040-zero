#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "pico.h"

/* 光照 —— 模拟输出接 ADC 脚。模块标称 TEMT6000、实为 CdS 光敏电阻，
 * 且带 ~1.75V 暗偏置电压（遮光降不到 0 的根因，见 light.c）。
 * 换算 = 带地板的幂律曲线，三参数可经串口 luxcal 调整并持久化（cfgstore）。
 * mv 返回原始毫伏（诊断用，可 NULL）。 */
#define LIGHT_CAL_A_DEFAULT      216.6f  /* lux = A × (x − floor)^GAMMA */
#define LIGHT_CAL_GAMMA_DEFAULT  0.6432f /* x = v/(Vref − v) */
#define LIGHT_CAL_FLOOR_DEFAULT  1.13f   /* x ≤ floor → 0 lux（暗电压地板） */

void light_init(uint adc_pin);
void light_set_cal(float a, float gamma, float floor_x); /* 运行时改（cfgstore 调） */
bool light_read(float *lux, uint32_t *mv);
