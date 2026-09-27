/* env-station（rp2040-zero 版）—— 温湿度 + 光照 + OLED 环境小站。
 *
 * 与 esp32-s3-zero/env-station 同名同构：dht/ssd1306 接口一致、#ENV 行协议
 * 同款（lux 为本板新增字段）；本板无无线电，遥测走 USB CDC（serialtap 采集）。
 * 实测接线：OLED SDA=GP12、SCL=GP13（I2C0，0x3C/0x3D 自适应，128x64）；
 * DHT11/22 DATA=GP27；TEMT6000 OUT=GP28（ADC2）；板载 WS2812=GP16 状态灯。
 *
 * UI（v0.3）：单页层级布局——页眉（标识+错误计数）/分隔线/温度大字/
 * RH+LUX 双列/分隔线/页脚（SEQ+运行时长）。
 * 周期性屏保（防烧屏规范，无按键板变体）：UI 10min → 星火动画 2min 循环。
 * 动画每拍随机点亮/熄灭一批像素、全屏均匀踩点——每个像素都有亮的机会，
 * 常亮的页眉/大字/页脚在屏保期整体休息；不再用纯黑屏/面板级熄屏。
 * 串口命令（USB CDC，serialtap 透传/代理皆可发）：定标免改固件——
 *   cal <toff> <rhoff>   温湿度屏显偏移（#ENV 上报原始值，WFP 约定）
 *   luxcal <A> <gamma>   光照幂律曲线（改换算本身，#ENV lux 跟随）
 *   cal? / luxcal? / help；参数写 flash 末扇区（cfgstore），断电保持
 * 节奏：DHT 2.5s/次（#ENV 跟随成功读）、光照 0.5s/次、OLED 0.25s 一刷
 * （屏保动画 120ms 一拍）、状态灯 250ms 一拍（绿呼吸=正常，红=DHT 失败，
 * 橙=OLED 失败）。
 * 看门狗 8s 在外设初始化【之前】启用（v0.1 教训：I2C 总线被拉死时
 * ssd1306_init 永久阻塞，而看门狗尚未武装 → 整机静默挂死）。
 * 开机先等 USB CDC 连接（最多 3s）再打诊断横幅，外设就绪情况一目了然。
 * 1200bps 触摸复位进 BOOTSEL（SDK 默认开）：免按 BOOT 刷机。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfgstore.h"
#include "dht.h"
#include "light.h"
#include "main.h"
#include "ssd1306.h"
#include "ws2812.h"

#include "hardware/i2c.h"
#include "hardware/watchdog.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#define OLED_ADDR 0x3C

/* 周期性屏保（防烧屏规范，无按键板变体）：UI 10min → 星火动画 2min 循环 */
#define SCREEN_UI_MS   (10 * 60 * 1000)
#define SCREEN_ANIM_MS (2 * 60 * 1000)
#define ANIM_TICK_MS   120

typedef enum { SCR_UI, SCR_ANIM } scr_phase_t;
static scr_phase_t s_scr = SCR_UI;
static absolute_time_t s_scr_deadline;

static dht_reading_t s_dht;
static int s_dht_err = 1;
static uint32_t s_seq, s_dht_errs;
static float s_lux;
static uint32_t s_lux_mv;
static bool s_light_ok, s_oled_ok;

/* 页眉 -线- 温度大字 / RH+LUX 双列 -线- 页脚 */
static void ui_render(void)
{
    char line[24];
    ssd1306_clear();
    ssd1306_text(0, 1, "ENV RP2040", 1);
    if (s_dht_errs > 0) { /* 错误计数右上角，有错才显示 */
        snprintf(line, sizeof line, "E%lu", (unsigned long)s_dht_errs);
        ssd1306_text(128 - (int)strlen(line) * 6, 1, line, 1);
    }
    ssd1306_hline(0, 127, 9);

    if (s_dht_err == 0) {
        /* 屏显 = 原始 + 定标偏移（cal 命令，cfgstore 持久化）；
         * #ENV 上报原始值——平台侧校准才是权威（WFP 约定） */
        snprintf(line, sizeof line, "%.1f\x80" "C",
                 (double)(s_dht.t_c + cfg_t_off()));
        ssd1306_text(2, 11, line, 3); /* 6 字 x18px = 108 → x2..110；负温 7 字恰满宽 */
        float rh = s_dht.rh + cfg_rh_off();
        if (rh < 0) rh = 0;
        if (rh > 100) rh = 100;
        snprintf(line, sizeof line, "%.0f%%", (double)rh);
        ssd1306_text(2, 35, line, 2); /* ≤4 字 x12 = 48 → x2..50 */
    } else {
        /* DHT ERR 态：大字错误 + 小字错误码/光照同行右对齐——错误串最长
         * "short-frame"(11 字 x6=66px → x2..68)，右侧留白 92..128 恰好放
         * 6 字的光照状态，互不重叠 */
        ssd1306_text(2, 11, "DHT ERR", 3); /* 7 字 x18 = 126 → x2..128 恰满 */
        ssd1306_text(2, 36, dht_err_str(s_dht_err), 1);
        snprintf(line, sizeof line, "%s", s_light_ok ? "lux ok" : "lux --");
        ssd1306_text(128 - (int)strlen(line) * 6, 36, line, 1);
    }
    if (s_dht_err == 0) {
        if (s_light_ok) {
            /* ≥1klx 用 klx 显示（右列 6 字 x12=72 → x54..126；9999 封顶会把
             * 上万的真实读数盖成同一个数——LDR 曲线在亮端是有效读数） */
            if (s_lux < 1000.0f) {
                snprintf(line, sizeof line, "%.0flx", (double)s_lux);
            } else if (s_lux < 10000.0f) {
                snprintf(line, sizeof line, "%.1fkl", (double)(s_lux / 1000.0f));
            } else {
                snprintf(line, sizeof line, "%.0fkl", (double)(s_lux / 1000.0f));
            }
        } else {
            snprintf(line, sizeof line, "--lx");
        }
        ssd1306_text(54, 35, line, 2); /* ≤6 字 x12 = 72 → x54..126 */
    }
    ssd1306_hline(0, 127, 54);

    /* 页脚：SEQ 左 / 运行时长右 */
    snprintf(line, sizeof line, "SEQ %lu", (unsigned long)s_seq);
    ssd1306_text(0, 56, line, 1);
    uint64_t up_s = to_ms_since_boot(get_absolute_time()) / 1000;
    snprintf(line, sizeof line, "UP %lu:%02lu",
             (unsigned long)(up_s / 3600), (unsigned long)((up_s / 60) % 60));
    ssd1306_text(128 - (int)strlen(line) * 6, 56, line, 1);
    ssd1306_flush();
}

/* 屏保动画：随机星火。每拍点亮 8 / 熄灭 64 个均匀随机像素（稳态亮 ~12%，
 * 每个像素等概率被踩点）——全屏无静止结构，常亮 UI 区整体休息。
 * 比纯黑好：黑屏阶段 OLED 虽不发光，但"每次都从同一帧亮起"本身就是静态
 * 图案；星火让全部像素轮流工作。 */
static void ss_anim_tick(void)
{
    for (int i = 0; i < 8; i++) {
        ssd1306_px_set(rand() % 128, rand() % 64, true);
    }
    for (int i = 0; i < 64; i++) {
        ssd1306_px_set(rand() % 128, rand() % 64, false);
    }
    ssd1306_flush();
}

/* —— 串口命令（定标免改固件）—— */
static char s_cmd[64];
static size_t s_cmd_len;

static void cmd_dispatch(const char *line)
{
    float a, b;
    if (sscanf(line, "cal %f %f", &a, &b) == 2) {
        printf("> cal t%+.1f rh%+.1f %s\n", (double)a, (double)b,
               cfg_set_trh(a, b) ? "saved" : "save FAILED(生效未持久)");
    } else if (!strcmp(line, "cal?")) {
        printf("> cal t%+.1f rh%+.1f\n",
               (double)cfg_t_off(), (double)cfg_rh_off());
    } else if (sscanf(line, "luxcal %f %f", &a, &b) == 2) {
        if (a <= 0 || b <= 0) {
            printf("> luxcal 参数需 > 0\n");
            return;
        }
        light_set_cal(a, b);
        printf("> luxcal A %.2f g %.3f %s\n", (double)a, (double)b,
               cfg_set_lux(a, b) ? "saved" : "save FAILED(生效未持久)");
    } else if (!strcmp(line, "luxcal?")) {
        printf("> luxcal A %.2f g %.3f（lux=A*(v/(3.3-v))^g；lmv 为原始毫伏）\n",
               (double)cfg_lux_a(), (double)cfg_lux_gamma());
    } else if (!strcmp(line, "help") || !strcmp(line, "?")) {
        printf("> cal <toff> <rhoff> | cal? | luxcal <A> <gamma> | luxcal? | help\n");
        printf("> cal 只改屏显（#ENV 报原始值）；luxcal 改 lux 换算（#ENV lux 跟随）；参数断电保持\n");
    } else if (line[0]) {
        printf("> 未知命令（help）\n");
    }
}

/* 主循环里轮询：收整行→回显→分发 */
static void cmd_poll(void)
{
    for (;;) {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            return;
        }
        if (c == '\r' || c == '\n') {
            if (s_cmd_len) {
                s_cmd[s_cmd_len] = 0;
                printf("\n");
                cmd_dispatch(s_cmd);
                s_cmd_len = 0;
                printf("> ");
            }
        } else if (c == '\b' || c == 127) {
            if (s_cmd_len) {
                s_cmd_len--;
                printf("\b \b");
            }
        } else if (c >= 32 && c < 127 &&
                   s_cmd_len < sizeof s_cmd - 1) {
            s_cmd[s_cmd_len++] = (char)c;
            putchar(c);
        }
    }
}

int main(void)
{
    stdio_init_all();
    /* 看门狗先于一切外设初始化武装：任何 init 挂死 8s 内整机复位自恢复 */
    watchdog_enable(8000, 1);

    /* 等 USB CDC 连上（最多 3s）再说话——否则横幅掉进无人接收的 FIFO */
    for (int i = 0; i < 30 && !stdio_usb_connected(); i++) {
        sleep_ms(100);
    }
    printf("[env-station] boot v0.1.0 (rp2040-zero)\n");

    cfg_init(); /* 定标参数（flash 持久化）→ 应用到光照曲线 */
    light_set_cal(cfg_lux_a(), cfg_lux_gamma());

    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    bool oled_ok = ssd1306_init(i2c0, OLED_ADDR);
    s_oled_ok = oled_ok;
    printf("[env] oled: %s (addr=0x%02X i2c_errs=%lu)\n",
           oled_ok ? "ok" : "FAIL", ssd1306_addr(),
           (unsigned long)ssd1306_errs());

    dht_init(DHT_PIN);
    light_init(LIGHT_ADC_PIN);
    printf("[env] dht: GP%d  light: GP%d (ADC2)\n", DHT_PIN, LIGHT_ADC_PIN);

    ws2812_init(WS2812_PIN);

    absolute_time_t t_dht = make_timeout_time_ms(1500); /* 上电延时首读 */
    absolute_time_t t_light = make_timeout_time_ms(500);
    absolute_time_t t_ui = get_absolute_time();
    absolute_time_t t_status = make_timeout_time_ms(60 * 1000);
    absolute_time_t t_anim = get_absolute_time();
    s_scr_deadline = make_timeout_time_ms(SCREEN_UI_MS);
    srand(to_ms_since_boot(get_absolute_time()) | 1);
    printf("> "); /* 命令提示符 */

    for (;;) {
        watchdog_update();
        absolute_time_t now = get_absolute_time();
        cmd_poll(); /* 串口命令随时可发（定标免改固件） */

        if (absolute_time_diff_us(now, t_dht) <= 0) {
            dht_reading_t r;
            int err = 0;
            if (dht_read(&r, &err)) {
                s_dht = r;
                s_dht_err = 0;
                /* #ENV 上报原始值（板端不做校准，平台侧校准才是权威——
                 * 与 S3 板同款约定，见 homepulse/docs/wfp-protocol.md） */
                printf("#ENV {\"seq\":%lu,\"t\":%.1f,\"rh\":%.1f,\"lux\":%.0f,"
                       "\"lmv\":%lu}\n",
                       (unsigned long)s_seq, (double)r.t_c, (double)r.rh,
                       (double)s_lux, (unsigned long)s_lux_mv);
                s_seq++;
            } else {
                s_dht_err = err;
                s_dht_errs++;
                printf("[env] DHT read fail: %s (errs=%lu)\n",
                       dht_err_str(err), (unsigned long)s_dht_errs);
            }
            t_dht = make_timeout_time_ms(DHT_PERIOD_MS);
        }

        if (absolute_time_diff_us(now, t_light) <= 0) {
            s_light_ok = light_read(&s_lux, &s_lux_mv);
            t_light = make_timeout_time_ms(LIGHT_PERIOD_MS);
        }

        /* 60s 周期状态行：开机横幅只打一次、落在 serialtap 暂停窗口就丢了
         * ——外设状态（OLED 地址/errs、lux 原始 mV、屏保态）随时可远程观测 */
        if (absolute_time_diff_us(now, t_status) <= 0) {
            printf("[env] status: seq=%lu dht_errs=%lu model=DHT%d "
                   "oled=0x%02X oled_errs=%lu lux_mv=%lu ss=%s\n",
                   (unsigned long)s_seq, (unsigned long)s_dht_errs, s_dht.model,
                   ssd1306_addr(), (unsigned long)ssd1306_errs(),
                   (unsigned long)s_lux_mv,
                   s_scr == SCR_ANIM ? "anim" : "ui");
            t_status = make_timeout_time_ms(60 * 1000);
        }

        if (absolute_time_diff_us(now, t_ui) <= 0) {
            ws2812_tick(s_dht_err == 0, s_oled_ok); /* 状态灯常走 */
            /* 周期性屏保：UI 10min → 星火动画 2min 循环（防烧屏规范） */
            if (absolute_time_diff_us(now, s_scr_deadline) <= 0) {
                s_scr = (s_scr == SCR_UI) ? SCR_ANIM : SCR_UI;
                printf("[env] screensaver -> %s\n",
                       s_scr == SCR_ANIM ? "anim" : "ui");
                s_scr_deadline = make_timeout_time_ms(
                    s_scr == SCR_ANIM ? SCREEN_ANIM_MS : SCREEN_UI_MS);
                if (s_scr == SCR_UI) {
                    ui_render(); /* 回 UI 立即刷一帧 */
                } else {
                    ssd1306_clear();
                    ssd1306_flush();
                }
            }
            if (s_scr == SCR_UI) {
                ui_render();
            }
            t_ui = make_timeout_time_ms(UI_PERIOD_MS);
        }
        if (s_scr == SCR_ANIM && absolute_time_diff_us(now, t_anim) <= 0) {
            ss_anim_tick(); /* 120ms 一拍，独立于 UI 刷新节奏 */
            t_anim = make_timeout_time_ms(ANIM_TICK_MS);
        }

        sleep_until(make_timeout_time_ms(50));
    }
}
