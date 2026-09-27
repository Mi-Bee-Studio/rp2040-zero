/* 板载 WS2812（GP16）状态灯 —— PIO 驱动（时序见 ws2812.pio）+ 两态心跳。
 * 绿色呼吸 = 采集正常；红色 2Hz 闪 = DHT 连续失败。视觉模式沿用 S3 板
 * "设备状态灯"惯例（serialtap/homepulse 将来可经链路下发颜色）。 */
#include "ws2812.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "ws2812.pio.h"

static PIO s_pio;
static uint s_sm;

void ws2812_init(uint pin)
{
    s_pio = pio0;
    uint offset = pio_add_program(s_pio, &ws2812_program);
    s_sm = pio_claim_unused_sm(s_pio, true);
    pio_gpio_init(s_pio, pin);
    pio_sm_set_consecutive_pindirs(s_pio, s_sm, pin, 1, true);
    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 32); /* 左移 MSB 先出，32bit 自动拉 */
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    /* 程序按 125MHz 默认主频定拍；若改主频请按比例改 ws2812.pio 注释 */
    sm_config_set_clkdiv(&c, 1.0f);
    pio_sm_init(s_pio, s_sm, offset, &c);
    pio_sm_set_enabled(s_pio, s_sm, true);
}

void ws2812_put(uint8_t r, uint8_t g, uint8_t b)
{
    /* GRB 序，左对齐到 32bit 顶（低 8 位为填充零） */
    uint32_t grb = ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
    pio_sm_put_blocking(s_pio, s_sm, grb);
    sleep_us(80); /* 帧复位：>50us 低电平锁存 */
}

void ws2812_tick(bool dht_ok, bool oled_ok)
{
    static int phase;
    static bool on;
    if (!oled_ok) {
        on = !on; /* 1Hz 慢闪（4 拍翻转） */
        if ((phase & 3) == 0) {
            ws2812_put(on ? 40 : 0, on ? 24 : 0, 0);
        }
        phase++;
        return;
    }
    if (dht_ok) {
        static const uint8_t ramp[8] = {2, 6, 14, 24, 24, 14, 6, 2};
        ws2812_put(0, ramp[phase], 0);
        phase = (phase + 1) & 7;
    } else {
        on = !on;
        ws2812_put(on ? 40 : 0, 0, 0);
    }
}
