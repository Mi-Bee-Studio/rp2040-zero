#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

/* 极简 SSD1306 128x64 OLED I2C 驱动（与 esp32-s3-zero/env-station 同构）：
 * 帧缓冲 + 5x7 字库文本 + 基本图元，够环境小站用。
 * v0.2：全部 I2C 写带 50ms 截止时间（供电/接线把总线拉死时快速失败，
 * 不再永久阻塞）；init 探测 0x3C/0x3D 两个常见地址。 */
bool ssd1306_init(i2c_inst_t *i2c, uint8_t addr);
uint8_t ssd1306_addr(void); /* 实际选中的地址（探测后；0=无应答） */
uint32_t ssd1306_errs(void); /* I2C 写失败累计（诊断用） */
void ssd1306_clear(void);
/* 在 (col, row) 处写字符串；scale=1 占 6x8 像素，scale=2 占 12x16 */
void ssd1306_text(int col, int row, const char *s, int scale);
void ssd1306_hline(int x0, int x1, int y);
void ssd1306_vline(int x, int y0, int y1);
void ssd1306_fill_rect(int x, int y, int w, int h);
/* 单像素读写（屏保动画用：随机点亮/熄灭走全屏均匀踩点） */
void ssd1306_px_set(int x, int y, bool on);
bool ssd1306_px_get(int x, int y);
/* 面板级开关（0xAE 关显示保持 GRAM，0xAF 恢复） */
void ssd1306_display_on(bool on);
void ssd1306_flush(void);
