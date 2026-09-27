# RP2040-Zero（Waveshare）

[中文文档](README.zh.md) | [English](README.md)

主板目录规范的一块板。**本目录按"主板为根"规范组织**：

```
rp2040-zero/
├── README.md          # 本文件：这块板的一切硬件信息
└── <project>/         # 每个用这块板做的项目一个目录（按能力命名）
    ├── CMakeLists.txt / main/ / pico_sdk_import.cmake   # RP2040 形态的构建三件套（pico-sdk）
    └── README.md      # 项目说明 + 编译/烧录命令
```

规范要点：

- **主板目录名** = 板子名（kebab-case），根 README 只写硬件、不写业务；
- **项目目录独立可编译**：按 RP2040 各自 SDK（官方 pico-sdk + CMake）自带构建三件套，
  配好 `PICO_SDK_PATH` 后 `cmake -B build && cmake --build build` 即出 UF2；
- 项目间不共享代码；需要共性时先拷贝，稳定后再考虑抽组件。

---

## 板子概要

| 项目 | 值 |
|------|-----|
| 模组/芯片 | RP2040 —— Arm Cortex-M0+ **双核** @133MHz，264KB 片内 SRAM |
| Flash | 2MB 外置 QSPI（**专用总线，不占 GPIO，未引出**） |
| 无线 | **无**（不带任何无线电——纯 USB 设备板；要联网需外接伴侣方案） |
| USB | **原生 USB 1.1（Type-C），无 USB-UART 桥芯片**；BOOTSEL 下载模式枚举为 `RPI-RP2` U盘（UF2 拖拽即刷） |
| 特色外设 | 2×UART · 2×SPI · 2×I2C · 16×PWM · 4×12bit ADC（GP26–29）+ 片内温度传感器 · **2× PIO（8 状态机，软定义时序）** |
| 板载 LED | **WS2812 RGB，GP16**（DIN；该脚未引出） |
| 按键 | BOOT（=BOOTSEL，进 UF2 下载，**非 GPIO**）、RESET |
| 调试 | 背面 SWD 焊点 ×3（SWCLK/SWDIO/GND）——openocd + Picoprobe/Debug Probe 可调可烧 |
| 供电 | USB-C 5V 输入；板载 LDO 出 3.3V；5V/3V3/GND 焊盘引出（3V3 带外设注意 LDO 余量） |
| 引出 | 正面三边邮票半孔 23（左 8 + 底 7 + 右 8）＋ 背面错位排 10 = 33 焊盘；**29 个用户 GPIO 全引出**（GP16 让给 WS2812） |
| 尺寸 | 18.00 × 23.50 mm（USB-C 在顶边），焊盘间距 2.54mm |

## 引脚位置图（USB-C 朝上，正面/元件面视角；引脚号同官方丝印）

```
                 ┌─ USB-C ─┐
        5V ◎┬───┘  [WS2812] ├───┬◎ GP0    ← SPI0 RX · I2C0 SDA · UART0 TX
       GND ◎│      (GP16)   │   ◎ GP1    ← SPI0 CSn · I2C0 SCL · UART0 RX
       3V3 ◎│               │   ◎ GP2    ← SPI0 SCK · I2C1 SDA
      GP29 ◎│ [BOOT] [RST]  │   ◎ GP3    ← SPI0 TX · I2C1 SCL
      GP28 ◎│               │   ◎ GP4    ← SPI0 RX · I2C0 SDA · UART1 TX
      GP27 ◎│    ┌─────┐    │   ◎ GP5    ← SPI0 CSn · I2C0 SCL · UART1 RX
      GP26 ◎│    │ RP2 │    │   ◎ GP6    ← SPI0 SCK · I2C1 SDA
      GP15 ◎│    │ 040 │    │   ◎ GP7    ← SPI0 TX · I2C1 SCL
           └───────────────┘
            底缘（左→右）：GP14 · GP13 · GP12 · GP11 · GP10 · GP9 · GP8

  背面（丝印面）错位半孔一排，位于右缘/底缘的间隙，自 USB 端起：
  GND · GP25 · GP24 · GP23 · GP22 · GP21 · GP20 · GP19 · GP18 · GP17
  （GP21/GP20 兼 I2C0；背面另有 SWD 三焊点；使用前核对背面丝印）
```

要点：

- **左排**自 USB 端向下：`5V, GND, 3V3, GP29, GP28, GP27, GP26, GP15`；
- **右排**自 USB 端向下：`GP0–GP7`；
- **底缘**（左→右）：`GP14, GP13, GP12, GP11, GP10, GP9, GP8`；
- **背面错位排**：`GND · GP25 · GP24 · GP23 · GP22 · GP21 · GP20 · GP19 · GP18 · GP17`（焊排针时正反面都焊）；
- **功能标注是官方默认映射，不是固定复用表**：RP2040 外设引脚走全矩阵，UART/SPI/I2C
  可重映射到几乎任何脚；2× PIO 的 8 个状态机更能自造任意时序——这是与 ESP 固定复用
  的最大差别；
- GP26–29 = ADC0–3（12bit，参考即 3V3 轨）＋ 片内温度传感器；GP16 = WS2812 DIN（未引出）；
- QSPI flash 专线不是 GPIO；BOOT 键是 BOOTSEL（硬件下载选择），无 ESP 式 strapping 陷阱。

## 注意事项

- **无 USB-UART 桥，也不是 USB-Serial-JTAG**：串口 = RP2040 原生 USB 1.1，
  **固件启用 CDC 后才有串口**（pico-sdk `stdio_usb` / TinyUSB CDC）；
  **BOOTSEL 模式下是 U 盘不是串口**（`RPI-RP2`，拖 UF2 即刷）——serialtap 的
  采集/透传链路取决于固件是否开了 CDC。
- **下载模式与 ESP 完全不同**：没有 esptool 那套串口复位协议——按住 BOOT 插 USB
  （或运行中按 RESET）进 BOOTSEL；正式刷机走 picotool 或 UF2 拷贝。板端固件也可以
  经 bootrom API 软重启进 BOOTSEL（免手按，体验对齐 esptool 自动复位）。
- **GPIO 电平 3.3V，不耐 5V**；没有 ESP 式 strapping 脚（BOOTSEL 是硬件选择线，
  不占 GPIO）。
- GP16 被 WS2812 占用；QSPI flash 专线非 GPIO——其余 GP0–15、GP17–29 共 29 个脚
  全部可用。
- 无独立 ADC_VREF 焊盘（Pico 有、本板未引出）：ADC 参考就是 3V3 轨，电源纹波直接
  进 ADC 读数，精密采样要权衡。

## Serialtap（中间层）适配点（真机已验，2026-09-27）

- 固件启用 USB CDC 后按普通 CDC 串口枚举（VID 2e8a，pico-sdk 默认 PID 000a，
  **序列号 = 板子 flash 唯一 ID 的大写 hex**，跨口跨固件稳定——语义名锚它即可）；
  serialtap 内置命名族 `rp2040-cdc`（0003/0005/000a），本机语义名 `rp2040-zero`；
- **⚠ DTR 适配（踩过的坑）**：pico-sdk 的 stdio_usb 以 DTR 判断"主机在听"，DTR
  未断言时**静默丢弃全部输出**；而 serialtap 默认 open 后释放 DTR/RTS（保护
  CH340/ESP 板）。解法 = serialtap 新增的 `dtr_hold` 配置：命中的设备 open 后
  保持 DTR+RTS——实测 #ENV 遥测正常进日志；
- **刷机已验两条路**：BOOTSEL + UF2 拖拽（RPI-RP2 U盘）／**1200bps 触摸软重启进
  BOOTSEL**（pico-sdk 默认开：对端口开 1200 波特即触发，serialtap 让口后可用，
  免按 BOOT）；`serialtap flash` 的 picotool 通道仍在能力地图待办（本板即试验场）；
- WS2812（GP16）可做"设备状态灯"，沿用 S3 板模式（serialtap/homepulse 经代理下发颜色）。

## 工具链

官方 **pico-sdk（C/C++）+ CMake + arm-none-eabi-gcc**（工作区惯例：RP2040 按各自
SDK）。本机已装机（2026-09-27）：SDK 在 `~/pico-sdk`（含 `lib/tinyusb` 子模块）、
交叉编译器在 `~/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1`（xPack GCC 15.2.1，
本机无包管理器装机路径，构建时把两者加入 PATH/`PICO_SDK_PATH`，见 env-station
README 的实测命令）。备选：MicroPython / Arduino——若走 MicroPython，构建三件套
约定按其形态另定。刷机三选一：BOOTSEL + UF2 拖拽 / `picotool load` / openocd（SWD）。

## 项目索引

| 项目 | 说明 |
|------|------|
| [env-station](env-station/README.zh.md) | 环境小站（RP2040 版，[S3 板同名项目](../esp32-s3-zero/env-station/README.zh.md)同构）：SSD1306 OLED（GP12/13 I2C0）+ DHT11/22（GP27）+ TEMT6000 光照（GP28）；#ENV JSON 行遥测走 USB CDC；WS2812 状态灯（兼 blink 基线）+ 硬件看门狗 |
