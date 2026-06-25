#include <Arduino.h>
#include <FastLED.h>
#include "nimble_mouse.h"

// --- 引脚定义（M5Stack ATOM Lite）---
#define LED_PIN     27   // 板载 WS2812 RGB LED
#define BUTTON_PIN  39   // 正面按键（ATOM Lite 板载已带上拉）
#define NUM_LEDS    1

// --- FastLED 设置 ---
CRGB leds[NUM_LEDS];

// --- 蓝牙鼠标实例（NimBLE）---
NimbleMouse bleMouse;

// --- 时间变量记录 ---
unsigned long lastMoveTime = 0;
unsigned long nextMoveInterval = 0;
unsigned long lastBlinkTime = 0;
unsigned long buttonPressStartTime = 0;

// --- 状态标志 ---
bool isLedOn = false;
bool isButtonPressed = false;

void setup() {
  Serial.begin(115200);

  // 1. 设置 CPU 频率为 80MHz 省电 (蓝牙最低要求 80MHz)
  setCpuFrequencyMhz(80);
  Serial.println("CPU Freq set to 80MHz");

  // 2. 初始化按键
  pinMode(BUTTON_PIN, INPUT);

  // 3. 初始化 WS2812 LED
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(20); // 调低亮度省电且不刺眼
  FastLED.clear();
  FastLED.show();

  // 4. 启动蓝牙鼠标 (设备名, 制造商, 电池电量)
  bleMouse.begin("RAPOO BT MOUSE", "RAPOO", 100);

  // 5. 初始化随机种子：用硬件真随机数 (WiFi/BT 开启后可用)，
  //    不再占用 GPIO32，避免与 Grove I2C(SCL=G32) 冲突
  randomSeed(esp_random());

  // 初始化首次移动的随机时间 (5-8分钟)
  // 5分钟 = 300,000 ms, 8分钟 = 480,000 ms
  nextMoveInterval = random(300000, 480000);
}

// 长按按键 5 秒：清除配对信息并重启
void clearPairingAndRestart() {
  Serial.println("Clearing pairing information...");
  leds[0] = CRGB::Red; // 重置时亮红灯提示
  FastLED.show();
  delay(1000);

  NimbleMouse::clearBonds(); // 删除全部蓝牙配对

  Serial.println("Rebooting...");
  ESP.restart();
}

void loop() {
  unsigned long currentMillis = millis();

  // ==================== 按键逻辑：长按5秒清除配对 ====================
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!isButtonPressed) {
      isButtonPressed = true;
      buttonPressStartTime = currentMillis;
    } else {
      if (currentMillis - buttonPressStartTime > 5000) {
        clearPairingAndRestart(); // 满足5秒，清除并重启
      }
    }
  } else {
    isButtonPressed = false;
  }

  // ==================== 状态机与 LED 指示 ====================
  if (bleMouse.isConnected()) {
    // 状态 3：已配对且已连接 -> 绿灯常亮
    leds[0] = CRGB::Green;
    FastLED.show();

    // ==================== 鼠标移动逻辑 ====================
    if (currentMillis - lastMoveTime > nextMoveInterval) {
      // 随机生成 X 和 Y 的移动像素 (2 到 5 像素)
      int moveX = random(2, 6);
      int moveY = random(2, 6);

      // 随机决定方向 (正或负)
      if (random(0, 2) == 0) moveX = -moveX;
      if (random(0, 2) == 0) moveY = -moveY;

      bleMouse.move(moveX, moveY);
      Serial.printf("Mouse moved: X:%d, Y:%d\n", moveX, moveY);

      // 记录时间并重新生成下一次的随机间隔
      lastMoveTime = currentMillis;
      nextMoveInterval = random(300000, 480000); // 再次生成 5-8 分钟的随机数
    }

  } else {
    // 未连接状态，判断是否曾经配对过
    if (NimbleMouse::bondCount() > 0) {
      // 状态 2：已配对但未连接 -> 蓝灯常亮
      leds[0] = CRGB::Blue;
      FastLED.show();
    } else {
      // 状态 1：没有任何配对 -> 蓝灯慢闪等待配对
      if (currentMillis - lastBlinkTime > 1000) { // 1秒切换一次状态 (慢闪)
        isLedOn = !isLedOn;
        leds[0] = isLedOn ? CRGB::Blue : CRGB::Black;
        FastLED.show();
        lastBlinkTime = currentMillis;
      }
    }
  }

  delay(10);
}
