#!/bin/bash
# build.sh — MouseJigger (M5Stack ATOM Lite) 固件编译 / 烧录
#
# 用法:
#   ./build.sh          # 仅编译
#   ./build.sh -f       # 编译 + 烧录（整套镜像）到 ATOM Lite
#   ./build.sh -w       # 不编译，直接烧录已有 mouseJigger.bin（整套镜像）
#
# 烧录目标写死为下面的 ATOM_DEV（by-id 路径，稳定不随枚举顺序变），从根上避免
# 误刷到 /dev/ttyUSB0 上的其它板子。多带的端口参数会被忽略。
#
# 板子说明：M5Stack ATOM Lite = ESP32-PICO-D4 / 4MB Flash / 无 PSRAM。
#   FQBN: m5stack:esp32:m5stack_atom（默认分区 huge_app: 3MB APP / 1MB SPIFFS）。

set -e

# 工具链路径可用环境变量覆盖（默认适配标准 Arduino 1.8.19 安装）
ARDUINO="${ARDUINO:-$HOME/Downloads/arduino-1.8.19/arduino}"
ESPTOOL="${ESPTOOL:-$HOME/.arduino15/packages/m5stack/tools/esptool_py/4.5.1/esptool.py}"
# 烧录波特率：这块 ATOM Lite 的 USB 串口芯片切换高速率(921600/460800)偶发丢数据，
# 默认用稳妥的 115200（约 1 分钟）。网络好的板子可用 BAUD=921600 ./build.sh -f 提速。
BAUD="${BAUD:-115200}"
CORE_DIR="${CORE_DIR:-$HOME/.arduino15/packages/m5stack/hardware/esp32/2.1.4}"
BOOT_APP0="${BOOT_APP0:-$CORE_DIR/tools/partitions/boot_app0.bin}"
# -w（不编译）时用 core 自带的预编译 bootloader + 分区表模板现场生成镜像，
# 这样即使板子上原来是别的分区布局（如 ESP-IDF 工程的 factory @0x20000）也能一次刷对。
BOOTLOADER_ELF="${BOOTLOADER_ELF:-$CORE_DIR/tools/sdk/esp32/bin/bootloader_dio_80m.elf}"
PART_CSV="${PART_CSV:-$CORE_DIR/tools/partitions/huge_app.csv}"
GEN_PART="${GEN_PART:-$CORE_DIR/tools/gen_esp32part.py}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH="$SCRIPT_DIR/mouseJigger.ino"

FQBN="m5stack:esp32:m5stack_atom"

BUILD_DIR="/tmp/mousejigger_build"
OUT_DIR="$SCRIPT_DIR"
BIN_NAME="mouseJigger.bin"

# ATOM Lite 稳定设备 ID（USB 串口 MAC 固定 → by-id 路径不随枚举顺序变）。
# 默认写死为在用的那块，避免误刷到 ttyUSB* 上的其它板子。
# 换板/多块同款板时用 ATOM_DEV=/dev/serial/by-id/... 覆盖。
ATOM_DEV="${ATOM_DEV:-/dev/serial/by-id/usb-Hades2001_M5stack_6552CC3093-if00-port0}"

DO_BUILD=1        # -w 关掉它：跳过编译，直接刷已有 .bin
FLASH_PORT=""

# ── 解析参数 ────────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    -f) FLASH_PORT="$ATOM_DEV"; shift ;;
    -w) DO_BUILD=0; FLASH_PORT="$ATOM_DEV"; shift ;;
    -h|--help)
      echo "用法: $0 [-f] | [-w]"
      echo "  (无参数)  仅编译"
      echo "  -f        编译并烧录（整套镜像）到写死的 ATOM Lite"
      echo "  -w        不编译，烧录已有 mouseJigger.bin（bootloader+分区表+app 整套）"
      exit 0 ;;
    *)
      if [[ "$1" == -* ]]; then echo "未知选项: $1"; exit 1; fi
      echo "注：忽略多余参数 '$1'（刷机设备已写死，无需端口）"; shift ;;
  esac
done

mkdir -p "$BUILD_DIR"

# ── 编译（-w 时跳过）─────────────────────────────────────────────────────────
if [[ $DO_BUILD -eq 1 ]]; then
  echo "=== 编译 MouseJigger 固件 ==="
  echo "    FQBN:   $FQBN"
  echo "    Sketch: $SKETCH"
  "$ARDUINO" --board "$FQBN" --pref "build.path=$BUILD_DIR" --verify "$SKETCH"

  BIN_SRC="$BUILD_DIR/mouseJigger.ino.bin"
  [[ -f "$BIN_SRC" ]] || { echo "ERROR: 没找到编译产物 $BIN_SRC"; exit 1; }
  cp "$BIN_SRC" "$OUT_DIR/$BIN_NAME"
  echo "=== 编译完成: $OUT_DIR/$BIN_NAME ($(stat -c%s "$OUT_DIR/$BIN_NAME") bytes) ==="
fi

# ── 烧录（可选）─────────────────────────────────────────────────────────────
if [[ -n "$FLASH_PORT" ]]; then
  APP_BIN="$OUT_DIR/$BIN_NAME"
  [[ -f "$APP_BIN" ]] || { echo "ERROR: 没找到 app 镜像 $APP_BIN"; exit 1; }

  if [[ $DO_BUILD -eq 1 ]]; then
    # 刚编译：刷整套镜像（bootloader/分区表/boot_app0/app）
    echo "=== 烧录整套镜像 -> $FLASH_PORT ==="
    WRITE_ARGS=(
      0x1000  "$BUILD_DIR/mouseJigger.ino.bootloader.bin"
      0x8000  "$BUILD_DIR/mouseJigger.ino.partitions.bin"
      0xe000  "$BOOT_APP0"
      0x10000 "$APP_BIN"
    )
  else
    # 不编译：用 core 现成件生成 bootloader / 分区表，同样刷整套。
    # 只刷 app 会踩坑——板子上残留的分区表可能把 app 指到别的偏移，刷完直接 boot loop。
    for f in "$BOOTLOADER_ELF" "$PART_CSV" "$GEN_PART" "$BOOT_APP0"; do
      [[ -f "$f" ]] || { echo "ERROR: 缺少 core 文件 $f"; exit 1; }
    done
    BOOTLOADER_BIN="$BUILD_DIR/bootloader_dio_80m.bin"
    PART_BIN="$BUILD_DIR/huge_app.partitions.bin"
    echo "=== 生成 bootloader / 分区表（huge_app）==="
    python3 "$ESPTOOL" --chip esp32 elf2image \
      --flash_mode dio --flash_freq 80m --flash_size 4MB \
      -o "$BOOTLOADER_BIN" "$BOOTLOADER_ELF"
    python3 "$GEN_PART" -q "$PART_CSV" "$PART_BIN"

    echo "=== 烧录整套镜像（未重新编译）-> $FLASH_PORT ==="
    WRITE_ARGS=(
      0x1000  "$BOOTLOADER_BIN"
      0x8000  "$PART_BIN"
      0xe000  "$BOOT_APP0"
      0x10000 "$APP_BIN"
    )
  fi

  python3 "$ESPTOOL" \
    --chip   esp32 \
    --port   "$FLASH_PORT" \
    --baud   "$BAUD" \
    --before default_reset \
    --after  hard_reset \
    write_flash -z \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 4MB \
    "${WRITE_ARGS[@]}"

  echo "=== 烧录完成 ==="
fi
