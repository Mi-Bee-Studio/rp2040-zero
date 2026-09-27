# env-station（rp2040-zero 版）—— 温湿度 + 光照 + OLED 环境小站

[English](README.md) | [中文文档](README.zh.md)

[esp32-s3-zero/env-station](../../esp32-s3-zero/env-station/README.zh.md) 的
RP2040-Zero 同构版：同名同构（`main.c` + `dht` + `ssd1306`，接口一致、
#ENV 行协议同款），无无线电，遥测走 USB CDC（serialtap 采集）。

## 接线（实测）

| 外设 | 引脚 | 说明 |
|------|------|------|
| SSD1306 128x64 OLED | **SDA=GP12、SCL=GP13**，3V3、GND | I2C0，地址 0x3C；GP12/13 是 I2C0 硬件默认对（偶=SDA、奇=SCL）；接反交换 `main.h` 两个定义即可 |
| DHT11/DHT22 | **DATA=GP27**，3V3、GND | 单总线；模块自带 4.7~10k 上拉（裸件需外加）；型号自动分辨 |
| 光照模块（标称 TEMT6000） | **OUT=GP28**，3V3、GND | **实为 CdS 光敏电阻模块**（两点定标实锤：57lux↔1837mV 与 ~505lux↔~2737mV 斜率差 6 倍，γ=1.615 是 LDR 特征）；换算 `lux = 39.4×(v/(3.3−v))^1.615`，≈4.8klux 起顶格；#ENV 带 `lmv` 原始毫伏可复核 |
| 板载 WS2812 | GP16（板上走线，无需接线） | 状态灯三态：绿呼吸=全好，红 2Hz 闪=DHT 失败，橙 1Hz 闪=OLED 失败 |

## 构建

需 pico-sdk（含 `lib/tinyusb` 子模块）+ arm-none-eabi-gcc + CMake ≥3.13。

本机实测配方（2026-09-27，Git Bash；工具链装在 `~/pico-sdk` 与
`~/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1`）：

```bash
export PICO_SDK_PATH=~/pico-sdk
export PATH=~/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:/c/Espressif/tools/ninja/1.12.1:$PATH
cmake -B build -S . -G Ninja
cmake --build build
# 产物：build/main/env-station.uf2（-Wall -Wextra 零告警）
```

其它环境（或 `PICO_SDK_FETCH_FROM_GIT=1` 自动拉 SDK）：

```bash
export PICO_SDK_PATH=~/pico-sdk    # 或 PICO_SDK_FETCH_FROM_GIT=1 自动拉取
cmake -B build -S . -G Ninja
cmake --build build
# 产物：build/main/env-station.uf2
```

## UI 与屏保

单页层级布局：页眉（`ENV RP2040` + 错误计数）— 温度大字（scale3，°C）—
RH / LUX 双列（scale2，单位自带语义）— 页脚（SEQ / 运行时长）。
**周期性屏保（防烧屏规范，无按键板变体）**：UI 10min → **星火动画 2min**
循环（`SCREEN_UI_MS/SCREEN_ANIM_MS` 可调）。动画每 120ms 随机点亮 8 / 熄灭
64 个均匀随机像素（稳态亮约 12%）——全屏无静止结构、每个像素等概率踩点，
常亮的页眉/大字/页脚在屏保期整体休息；不再用纯黑屏（黑屏阶段"每次从同一
帧亮起"仍是静态图案）。

## 烧录

- **BOOTSEL**：按住 BOOT 插 USB → 出现 `RPI-RP2` U盘 → 拖入
  `env-station.uf2`；
- **1200bps 触摸**（固件在线时免按 BOOT）：对端口以 1200 波特打开一下即软重启进
  BOOTSEL（pico-sdk 默认开）。serialtap 采集下先 `serialtap pause '^rp2040-zero$'`
  让口再触摸；
- **picotool**：`picotool load build/main/env-station.uf2 -fx`。

**serialtap 采集须知**：本固件走 pico-sdk stdio_usb，以 DTR 判断"主机在听"——
serialtap 需在配置里把本设备列入 `dtr_hold`（open 后保持 DTR+RTS），否则固件
静默无输出（板 README「适配点」有完整记录）。

## 运行

- 节奏：DHT 2.5s/次、光照 0.5s/次、OLED 0.25s 一刷、状态灯 250ms 一拍；
- **#ENV 遥测**（每 2.5s，成功读时）：

  ```
  #ENV {"seq":12,"t":24.6,"rh":58.1,"lux":312,"lmv":1560}
  ```

  `lmv` = 光照通道原始毫伏（GP28 电压），供换算核对/定标。

  与 S3 板同款 JSON 行（`lux` 为本板新增字段）；板端不做校准，平台侧校准
  才是权威（见 homepulse/docs/wfp-protocol.md）。
- **周期状态行**（每 60s）：`[env] status: seq=.. dht_errs=.. model=DHT11
  oled=0x3C oled_errs=.. lux_mv=.. ss=on`——开机横幅只打一次（可能落在
  serialtap 暂停窗口里丢掉），外设状态靠它随时可远程观测；
- **型号自动分辨（量程回退）**：先按 DHT22 解，解出物理量程外（rh>100 或
  温度越界）自动回退 DHT11——单看"小数字节非零"会误判（DHT11 的十位
  小数字节非零恰好像 DHT22 格式，曾把本板 DHT11 解成 rh=998%/t=922℃，
  2026-09-27 修正，S3 板同款代码已同步）；
- **诊断行**：`[env] DHT read fail: timeout (errs=3)`；
- **状态灯三态**：绿呼吸=全好 / 红 2Hz 闪=DHT 失败 / 橙 1Hz 闪=OLED 失败
  （屏上无从报错，灯是唯一通道）；
- **OLED**：见「UI 与屏保」——DHT 失败态为大字 `DHT ERR` + 错误码小字 +
  右对齐光照状态（排版保证互不重叠、不出屏）；
- **看门狗**：硬件 8s，主循环喂狗，卡死复位自恢复。

## 串口命令（定标免改固件）

经 USB CDC（serialtap 采集流/代理端点皆可发），行式命令，带回显与 `> ` 提示符：

| 命令 | 作用 |
|------|------|
| `cal <toff> <rhoff>` | 温湿度**屏显**偏移（°C/%RH；`#ENV` 上报原始值——平台侧校准才是权威，WFP 约定） |
| `luxcal <A> <gamma>` | 光照幂律曲线参数：`lux = A×(v/(3.3−v))^gamma`（改换算本身，`#ENV` 的 lux 跟随，`lmv` 永远是原始毫伏） |
| `cal?` / `luxcal?` | 查询当前值 |
| `help` | 命令列表 |

参数写入 flash 末 4K 扇区（cfgstore：magic+CRC32，无效自动回默认），
**断电保持、重刷固件不丢**；设置即落盘并立即生效。
定标操作建议：改 `luxcal` 前先用 AS803 等仪表在传感器同位置测照度，
调 A（整体比例）/gamma（曲线形状）直到读数吻合。

## 排障

| 症状 | 处理 |
|------|------|
| OLED 全黑 | 查地址（多数 0x3C，个别 0x3D）、SDA/SCL 是否接反；若为 128x32 屏，改 `ssd1306.c` 初始化 `0xA8,0x1F`、`0xDA,0x02` |
| DHT 一直 timeout | 查上拉与供电；`[env]` 行的 errs 计数确认；DHT 损坏常见为恒拉低 |
| LUX 顶格不动 | 正常饱和（量程 ≈360lux）；需要大范围量程换 BH1750 之类数字传感器 |
| 串口无输出 | 固件 CDC 需枚举后 ~1s；确认工具连的是新的 COM 口（BOOTSEL 模式下是 U 盘不是串口） |
