/* cfgstore.c —— 定标参数 flash 持久化（实现说明见 cfgstore.h）。 */
#include "cfgstore.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "light.h" /* LIGHT_CAL_*_DEFAULT */

#define CFG_MAGIC   0x43414C31u /* 'CAL1' */
#define CFG_VERSION 2u /* v2: +lux 暗地板 f（结构变更，旧数据自动回默认） */
/* 板载 2MB flash 末扇区；PICO_FLASH_SIZE_BYTES 缺省即 2MB，双保险取小 */
#ifndef CFG_FLASH_OFFSET
#define CFG_FLASH_OFFSET (2 * 1024 * 1024 - FLASH_SECTOR_SIZE)
#endif

typedef struct {
    uint32_t magic;
    uint16_t version, pad;
    float toff, rhoff;
    float lux_a, lux_g, lux_f;
    uint32_t crc;
} cfg_blob_t;

static cfg_blob_t s_cfg = {
    .magic = CFG_MAGIC, .version = CFG_VERSION,
    .toff = 0.0f, .rhoff = 0.0f,
    .lux_a = LIGHT_CAL_A_DEFAULT, .lux_g = LIGHT_CAL_GAMMA_DEFAULT,
    .lux_f = LIGHT_CAL_FLOOR_DEFAULT,
};

static uint32_t crc32_calc(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

static bool blob_valid(const cfg_blob_t *b)
{
    return b->magic == CFG_MAGIC && b->version == CFG_VERSION &&
           b->crc == crc32_calc((const uint8_t *)b,
                                sizeof *b - sizeof b->crc);
}

void cfg_init(void)
{
    cfg_blob_t stored;
    memcpy(&stored, (const void *)(XIP_BASE + CFG_FLASH_OFFSET),
           sizeof stored);
    if (blob_valid(&stored)) {
        s_cfg = stored;
        printf("[cfg] loaded: t%+.1f rh%+.1f luxA %.2f g %.4f f %.3f\n",
               (double)s_cfg.toff, (double)s_cfg.rhoff,
               (double)s_cfg.lux_a, (double)s_cfg.lux_g,
               (double)s_cfg.lux_f);
    } else {
        printf("[cfg] defaults（flash 无有效校准数据）\n");
    }
}

bool cfg_save(void)
{
    s_cfg.crc = crc32_calc((const uint8_t *)&s_cfg,
                           sizeof s_cfg - sizeof s_cfg.crc);
    /* flash 编程两条硬约束（官方 flash_program 例程模式，v0.4.0 两个都踩过）：
     * ① 编程期间 XIP 停摆，所有中断（USB/定时器，处理代码都在 flash）必须
     *    先关——否则 IRQ 一触发就 bus fault → 看门狗重启循环（实测复现）；
     * ② flash_range_program 的 count 必须是 256 的倍数——28 字节结构体
     *    补 0xFF 到整页再写。 */
    static_assert(sizeof s_cfg <= 256, "cfg blob must fit one 256B page");
    static uint8_t page[256];
    memcpy(page, &s_cfg, sizeof s_cfg);
    memset(page + sizeof s_cfg, 0xFF, sizeof page - sizeof s_cfg);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(CFG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CFG_FLASH_OFFSET, page, sizeof page);
    restore_interrupts(ints);
    cfg_blob_t readback;
    memcpy(&readback, (const void *)(XIP_BASE + CFG_FLASH_OFFSET),
           sizeof readback);
    if (!blob_valid(&readback)) {
        printf("[cfg] save FAILED（回读校验不符）\n");
        return false;
    }
    return true;
}

float cfg_t_off(void) { return s_cfg.toff; }
float cfg_rh_off(void) { return s_cfg.rhoff; }
float cfg_lux_a(void) { return s_cfg.lux_a; }
float cfg_lux_gamma(void) { return s_cfg.lux_g; }
float cfg_lux_floor(void) { return s_cfg.lux_f; }

bool cfg_set_trh(float toff, float rhoff)
{
    s_cfg.toff = toff;
    s_cfg.rhoff = rhoff;
    return cfg_save();
}

bool cfg_set_lux(float a, float gamma, float floor_x)
{
    s_cfg.lux_a = a;
    s_cfg.lux_g = gamma;
    s_cfg.lux_f = floor_x;
    return cfg_save();
}
