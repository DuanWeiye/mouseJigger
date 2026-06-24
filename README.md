# MouseJigger — ESP32 蓝牙鼠标随机移动器

基于 **ESP32** 的低功耗蓝牙（BLE）鼠标模拟器：每隔 **5～8 分钟**随机微移鼠标几个像素，
让电脑误以为有人在操作，从而**防止进入休眠/锁屏/离开状态**。

设备会被电脑识别为一个普通的蓝牙鼠标（默认名称 `RAPOO BT MOUSE`），无需安装任何驱动或软件，
真正做到「插上即用、对系统完全透明」。

> ✅ **默认目标型号：[M5Stack ATOM Lite](https://docs.m5stack.com/zh_CN/core/atom_lite)**
> 固件的引脚配置（板载 RGB LED 与正面按键）完全对应 ATOM Lite，**开箱即烧、无需任何改动或额外接线**。
> 使用其它 ESP32 开发板时，请参考下文「[换用其它 ESP32 开发板](#️-换用其它-esp32-开发板需自行修改)」自行调整配置。

---

## ✨ 特性

- 🖱️ **真·硬件鼠标**：以标准 BLE HID 鼠标身份连接，系统无法区分真假，不会被防作弊/合规软件标记为脚本。
- 🎲 **随机抖动**：每 5～8 分钟随机移动 2～5 像素，X/Y 方向随机，避免规律性。
- 💡 **RGB 状态灯**：板载 WS2812 指示当前配对/连接状态，一眼可知工作情况。
- 🔋 **省电设计**：CPU 降频至 80MHz（BLE 最低要求），LED 低亮度。
- 🔘 **一键清除配对**：长按按键 5 秒即可清空蓝牙配对信息，方便换电脑配对。

---

## 🧰 硬件需求

### 默认硬件：M5Stack ATOM Lite（开箱即用）

[M5Stack ATOM Lite](https://docs.m5stack.com/zh_CN/core/atom_lite) 主控为 ESP32-PICO-D4，
自带本固件所需的全部外设，**无需任何额外接线或改代码**：

- 1 颗板载 WS2812C RGB LED（GPIO27）
- 1 个正面按键（GPIO39，板载已带上拉，可直接读取）

### 引脚分配（ATOM Lite 默认）

| 功能 | GPIO | 备注 |
|---|---|---|
| WS2812 LED 数据 | `27` | ATOM Lite 板载 RGB LED |
| 按键 | `39` | ATOM Lite 正面按键（板载已带上拉） |
| 随机数种子（悬空 ADC） | `32` | 仅用于取随机噪声，无需接线 |

### ⚠️ 换用其它 ESP32 开发板需自行修改

若你用的**不是 ATOM Lite**，而是其它 ESP32 开发板，请按实际硬件
**修改 `mouseJigger.ino` 顶部的引脚宏定义**（`LED_PIN` / `BUTTON_PIN`），并特别注意以下两点：

- **按键引脚**：本固件按「按下 = 低电平」读取。
  GPIO34～39 是 ESP32 的**输入专用引脚，内部没有上拉/下拉电阻**。
  - ATOM Lite 的按键（GPIO39）板载已带上拉，所以默认能直接用；
  - 若你的板子按键接在 34～39 且**无板载上拉**，需外接上拉电阻（建议 10kΩ 接 3.3V，按键另一端接 GND），否则引脚悬空可能误触发「清除配对」；
  - 或者把按键改接到普通 GPIO（如 25/26/33 等），并把代码里的 `pinMode(BUTTON_PIN, INPUT)` 改成 `pinMode(BUTTON_PIN, INPUT_PULLUP)`，即可省去外接电阻。
- **LED**：若你的板子没有 WS2812，需另接一颗，或改用对应的 LED 驱动方式（修改 `FastLED.addLeds<...>` 的芯片/颜色顺序）。

---

## 💡 LED 状态指示

| 颜色 | 状态 | 含义 |
|---|---|---|
| 🔵 蓝灯慢闪（1 秒） | 未配对 | 正在等待电脑发现并配对 |
| 🔵 蓝灯常亮 | 已配对，未连接 | 曾配对过，但当前未连上 |
| 🟢 绿灯常亮 | 已连接 | 正常工作中，定时抖动鼠标 |
| 🔴 红灯（约 1 秒） | 清除配对 | 长按按键 5 秒后亮起，随即重启 |

---

## 🔘 按键操作

- **长按 5 秒**：清除全部蓝牙配对信息（擦除 NVS 分区）并自动重启，
  之后即可与新的电脑重新配对。

---

## 📦 软件依赖

| 依赖 | 用途 | 来源 |
|---|---|---|
| [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse) | BLE HID 鼠标 | T-vK，MIT |
| [FastLED](https://github.com/FastLED/FastLED) | 驱动 WS2812 | MIT |
| Arduino-ESP32 核心 | ESP32 开发板支持 | Espressif |

> 仓库内附带了 `ESP32-BLE-Mouse.zip`（来自 [T-vK/ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse)，MIT 协议），
> 方便直接通过 Arduino IDE 的「项目 → 加载库 → 添加 .ZIP 库」安装。版权归原作者所有。

---

## 🚀 编译与烧录

1. 在 Arduino IDE 中安装 **ESP32 开发板支持**（开发板管理器搜索 `esp32`）。
2. 安装库：
   - 通过库管理器搜索安装 **FastLED**；
   - 通过「项目 → 加载库 → 添加 .ZIP 库」加载本仓库的 `ESP32-BLE-Mouse.zip`。
3. 打开 `mouseJigger.ino`，选择开发板与串口
   （ATOM Lite 选择开发板 **M5Stack-ATOM** 或 **M5Atom**；其它板子选对应型号）。
4. 点击上传。
5. 上传后，电脑蓝牙设置里搜索并连接名为 `RAPOO BT MOUSE` 的设备即可。

---

## ⚙️ 自定义

可在 `mouseJigger.ino` 顶部或对应位置调整：

- **抖动间隔**：搜索 `random(300000, 480000)`（单位毫秒，当前为 5～8 分钟）。
  若你的电脑休眠时间较短，可改小，例如 `random(60000, 120000)`（1～2 分钟）。
- **移动幅度**：搜索 `random(2, 6)`（当前 2～5 像素）。
- **设备名称/厂商**：修改 `BleMouse bleMouse("RAPOO BT MOUSE", "RAPOO", 100);`。
- **引脚**：修改顶部 `LED_PIN` / `BUTTON_PIN` 宏定义。

---

## 📄 许可证

本项目采用 [MIT License](LICENSE)。

第三方库 `ESP32-BLE-Mouse`、`FastLED` 各自遵循其原始许可证（均为 MIT），版权归原作者所有。
