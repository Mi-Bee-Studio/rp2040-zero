/* 光照采集 —— OUT→GP28（ADC2），3V3 供电。
 *
 * 模块身份（2026-09-27 定标实锤）：标称 TEMT6000，实为 **CdS 光敏电阻模块
 * 且带 ~1.75V 暗偏置电压**（LDR 支路 + 未知偏置结构，亮→电压升）。
 * 证据：AS803 照度计三点——遮光 0lux↔~1750mV、57lux↔1837mV、
 * ~505lux↔~2737mV——斜率差 6 倍排除线性光敏晶体管；带地板幂律拟合
 * γ=0.643 落在 CdS 特征区（0.5~0.7），三点误差 <1%。
 *
 * 换算（三点定标，无需知道 Rfix）：
 *   x = v/(3.3−v)；**lux = A × (x − floor)^γ**，x ≤ floor（暗地板）→ 0 lux。
 *   默认 A=216.6、γ=0.6432、floor=1.13（对应 v≈1.75V）。
 * 量程：v→3.25V 起 ≈11klux 后趋于顶格（lmv 恒 ≈3300）。
 * 遮光读数若仍偏高（地板漂移），用 `luxcal A gamma floor` 重定标：
 * floor 取"完全遮光时的 x 值"（x = lmv/(3300−lmv)）。
 */
#include "light.h"

#include <math.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"

#define LIGHT_OVERSAMPLE 16
#define LIGHT_VREF_MV    3300.0f
#define LIGHT_SAT_MV     3290    /* ≥此值视为顶格（极亮或模块异常） */
#define LIGHT_MAX_LUX    99999.0f

static float s_cal_a = LIGHT_CAL_A_DEFAULT;     /* 曲线参数（luxcal 可改） */
static float s_cal_g = LIGHT_CAL_GAMMA_DEFAULT;
static float s_cal_f = LIGHT_CAL_FLOOR_DEFAULT;

void light_set_cal(float a, float gamma, float floor_x)
{
    s_cal_a = a;
    s_cal_g = gamma;
    s_cal_f = floor_x;
}

/* GP26/27/28/29 = ADC0/1/2/3；本板 GP28 = ADC2 */
#define LIGHT_ADC_INPUT 2

void light_init(uint adc_pin)
{
    adc_init();
    adc_gpio_init(adc_pin);
}

bool light_read(float *lux, uint32_t *mv)
{
    uint32_t acc = 0;
    for (int i = 0; i < LIGHT_OVERSAMPLE; i++) {
        adc_select_input(LIGHT_ADC_INPUT);
        acc += adc_read();
        sleep_us(100);
    }
    uint32_t milliv = acc * 3300u / (LIGHT_OVERSAMPLE * 4095u);
    if (mv) {
        *mv = milliv;
    }
    if (milliv >= LIGHT_SAT_MV) {
        *lux = LIGHT_MAX_LUX; /* 顶格（极亮或模块异常） */
        return true;
    }
    float v = (float)milliv;
    float x = v / (LIGHT_VREF_MV - v);       /* = Rfix/R_ldr 比例量 */
    if (x <= s_cal_f) {                      /* 暗电压地板以下 = 0 lux */
        *lux = 0.0f;
        return true;
    }
    float l = s_cal_a * powf(x - s_cal_f, s_cal_g);
    *lux = l > LIGHT_MAX_LUX ? LIGHT_MAX_LUX : l;
    return true;
}
