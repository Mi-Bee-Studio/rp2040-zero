#pragma once
#include <stdbool.h>

/* cfgstore —— 板级定标参数的 flash 持久化（rp2040 无 NVS/EEPROM）。
 *
 * 存储位置：flash 末 4K 扇区（app ~100KB 远在其前，刷固件/拷 UF2 都不动它）。
 * 结构 magic+version+CRC32，无效（首次上电/损坏）即用默认值。
 * 串口命令 cal / luxcal 修改后立即落盘——免改固件定标，断电保持。
 *
 * 命令速查（详见 main.c cmd_dispatch）：
 *   cal <toff> <rhoff>    温湿度屏显偏移（#ENV 上报原始值——WFP 约定，
 *                         平台侧校准才是权威；屏显 = 原始+偏移）
 *   luxcal <A> <gamma>    光照幂律曲线 lux = A×(v/(3.3−v))^gamma
 *                         （改换算本身：#ENV 的 lux 跟着变，lmv 永远是原始毫伏）
 *   cal? / luxcal?        查询；help 命令列表
 */

void cfg_init(void);   /* 加载（无有效存储则默认）并应用 */
bool cfg_save(void);   /* 擦写扇区并回读校验（~百 ms；主循环内调用安全，狗 8s 足够） */

float cfg_t_off(void);
float cfg_rh_off(void);
float cfg_lux_a(void);
float cfg_lux_gamma(void);

/* 返回 false = 写入或回读校验失败（参数仍已生效，仅未持久化） */
bool cfg_set_trh(float toff, float rhoff);
bool cfg_set_lux(float a, float gamma);
