#pragma once

/* 实测接线（与 esp32-s3-zero/env-station 同名同构；rp2040-zero 板）：
 *   SSD1306 128x64 OLED  I2C0：SDA=GP12、SCL=GP13（0x3C，地址不匹配看排障节）
 *   DHT11/DHT22 温湿度    DATA=GP27（模块自带/外部 4.7~10k 上拉）
 *   TEMT6000 光照         OUT=GP28（=ADC2；模块 10k 负载，约 9.1mV/lux）
 * 板载：WS2812=GP16（状态灯）、BOOT=BOOTSEL、USB-C（CDC 遥测/日志）
 * 若 OLED 两根线接反，交换下面两个定义即可（I2C0 偶=SDA、奇=SCL 是硬件默认对）。 */
#define I2C_SDA_PIN    12
#define I2C_SCL_PIN    13
#define DHT_PIN        27
#define LIGHT_ADC_PIN  28
#define WS2812_PIN     16

#define DHT_PERIOD_MS   2500 /* DHT 两次读取至少隔 1s，2.5s 舒适档 */
#define LIGHT_PERIOD_MS  500
#define UI_PERIOD_MS     250
