/* 光照采集 —— OUT→GP28（ADC2），3V3 供电。
 *
 * 模块身份（2026-09-27 两点定标实锤）：标称 TEMT6000，实为 **CdS 光敏电阻
 * 模块**（LDR，VCC—LDR—OUT—Rfix—GND，亮→电压升）。证据：57lux↔1837mV 与
 * ~505lux↔~2737mV（AS803 照度计两点）斜率差 6 倍，线性光敏晶体管不可能；
 * 幂律拟合 γ=1.615 恰在 CdS 特征区（0.5~0.7），反推暗阻 ~23kΩ@10lux ≈
 * GL5528 规格。
 *
 * 换算（两点定标，无需知道 Rfix）：**lux = 39.4 × (v/(3.3−v))^1.615**。
 * 量程：v→3.25V 时 ≈4.8klux 起趋于饱和（lmv 恒 ≈3300 = 顶格）；
 * 极暗 v→0 自然收敛到 0。
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

void light_set_cal(float a, float gamma)
{
    s_cal_a = a;
    s_cal_g = gamma;
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
    float l = s_cal_a * powf(x, s_cal_g);
    *lux = l > LIGHT_MAX_LUX ? LIGHT_MAX_LUX : l;
    return true;
}
