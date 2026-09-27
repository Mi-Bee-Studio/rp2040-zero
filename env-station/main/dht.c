/* DHT11/DHT22 单总线读取 —— GPIO 位带采样（RP2040 版；S3 版用 RMT，本板无 RMT）。
 *
 * 时序同 S3 版：起始拉低 20ms（推挽低 → 切输入交还上拉，开漏语义无竞争）
 * → 传感器回 80us 低 + 80us 高 → 40 bit：每 bit 50us 低 + 26us 高(0) / 70us 高(1)。
 *
 * 捕获窗（~25ms）内关中断轮询边沿：最小脉宽 15us，125MHz 下轮询采样绰绰
 * 有余；窗内 USB CDC 短暂静默（~6ms 有效波形）对链路无影响。
 *
 * 解析与 S3 版逐字同构：收集 15~120us 高电平——首枚是释放伪影/80us 响应，
 * 其后 40 枚是数据位（>45us 判 1）；小数字节分辨 DHT22/DHT11。
 * 引脚要求外部 4.7~10k 上拉（模块自带），内部上拉兜底。
 */
#include "dht.h"

#include <string.h>

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

static int s_gpio = -1;

void dht_init(int gpio_num)
{
    s_gpio = gpio_num;
    gpio_init(gpio_num);
    gpio_set_dir(gpio_num, GPIO_IN);
    gpio_pull_up(gpio_num);
}

bool dht_read(dht_reading_t *out, int *err)
{
    memset(out, 0, sizeof(*out));
    int e = 0;

    /* 起始脉冲：拉低 20ms 后切回输入，上拉接管 */
    gpio_set_dir(s_gpio, GPIO_OUT);
    gpio_put(s_gpio, 0);
    sleep_ms(20);

    uint32_t save = save_and_disable_interrupts();
    gpio_set_dir(s_gpio, GPIO_IN);
    gpio_pull_up(s_gpio);

    int highs[48] = { 0 };
    int nh = 0;
    int n_sym = 0;
    bool last = true;
    uint64_t t_win = time_us_64();
    uint64_t t_edge = t_win;
    for (;;) {
        uint64_t now = time_us_64();
        if (now - t_win > 25000) { /* 25ms 捕获窗（一轮有效波形 ~5ms） */
            break;
        }
        bool lvl = gpio_get(s_gpio);
        if (lvl != last) {
            int dur = (int)(now - t_edge);
            if (last && dur >= 15 && dur <= 120 && nh < 48) {
                highs[nh++] = dur;
            }
            n_sym++;
            last = lvl;
            t_edge = now;
        }
    }
    restore_interrupts(save);

    out->n_highs = nh;
    out->n_sym = n_sym;
    if (nh == 0) {
        e = 1; /* 总线无任何边沿（传感器未接/无供电/线被按死） */
        goto done;
    }
    /* nh=42：释放伪影(~30us)+响应(80us)+40 位；nh=41：响应+40 位 */
    const int *bits_src = highs;
    int nbits = nh;
    if (nbits == 42) {
        bits_src = highs + 2;
        nbits = 40;
    } else if (nbits == 41) {
        bits_src = highs + 1;
        nbits = 40;
    }
    if (nbits < 40) {
        e = 3; /* 总线有边沿但位数不足（波形/上拉问题） */
        goto done;
    }

    uint64_t v = 0;
    for (int i = 0; i < 40; i++) {
        v = (v << 1) | (bits_src[i] > 45 ? 1 : 0);
    }
    for (int i = 0; i < 5; i++) {
        out->raw[i] = (v >> (32 - 8 * i)) & 0xFF;
    }
    uint8_t sum = out->raw[0] + out->raw[1] + out->raw[2] + out->raw[3];
    if (sum != out->raw[4]) {
        e = 2; /* 校验和错 */
        goto done;
    }

    /* 型号分辨（v0.3）：先按 DHT22 解，物理量程外（rh>100 / |t| 越界）回退
     * DHT11。单看"小数字节非零"会误判——DHT11 的十位小数字节非零恰好长得
     * 像 DHT22 格式：实测帧 27 00 24 03 按 DHT22 解出 rh=998.4%/t=921.9℃，
     * 按 DHT11 解出 rh=39%/t=36.3℃（2026-09-27 rp2040-zero 垃圾值根因）。
     * 真 DHT22 的 rh 原始值上限 0x03E8(=100.0)，rh>100 必非 DHT22。 */
    float rh22 = ((out->raw[0] << 8) | out->raw[1]) * 0.1f;
    float t22 = (((out->raw[2] & 0x7F) << 8) | out->raw[3]) * 0.1f;
    if (out->raw[2] & 0x80) {
        t22 = -t22;
    }
    bool dht22 = (out->raw[1] != 0 || out->raw[3] != 0)
                 && rh22 <= 100.0f && t22 <= 125.0f && t22 >= -40.0f;
    out->model = dht22 ? 22 : 11;
    if (dht22) {
        out->rh = rh22;
        out->t_c = t22;
    } else {
        out->rh = out->raw[0] + out->raw[1] * 0.1f;
        out->t_c = out->raw[2] + out->raw[3] * 0.1f;
    }
    out->ok = true;

done:
    if (err) {
        *err = e;
    }
    return out->ok;
}

const char *dht_err_str(int err)
{
    switch (err) {
    case 0: return "ok";
    case 1: return "timeout";
    case 2: return "checksum";
    case 3: return "short-frame";
    default: return "?";
    }
}
