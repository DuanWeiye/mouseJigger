#include <Arduino.h>
#include <FastLED.h>
#include "nimble_mouse.h"
#include "settings.h"
#include "env.h"
#include "net.h"
#include "web.h"

// --- 引脚定义（M5Stack ATOM Lite）---
#define LED_PIN     27   // 板载 WS2812 RGB LED
#define BUTTON_PIN  39   // 正面按键（ATOM Lite 板载已带上拉）
#define NUM_LEDS    1

// --- FastLED 设置 ---
CRGB leds[NUM_LEDS];

// --- 蓝牙鼠标实例（NimBLE）---
NimbleMouse bleMouse;

// --- 供 web.h 读取的蓝牙状态（每 loop 更新）---
bool g_bleConnected = false;
int  g_bleBond      = 0;
bool g_needReboot   = false;   // 设备名变更后置位，主循环择机重启

// --- 时间变量记录 ---
unsigned long lastMoveTime = 0;
unsigned long nextMoveInterval = 0;
unsigned long lastBlinkTime = 0;
unsigned long buttonPressStartTime = 0;
unsigned long lastEnvRead = 0;

// --- 状态标志 ---
bool isLedOn = false;
bool isButtonPressed = false;

void setup() {
  Serial.begin(115200);

  // 1. 设置 CPU 频率为 80MHz 省电 (蓝牙最低要求 80MHz)
  setCpuFrequencyMhz(80);

  // 2. 初始化按键
  pinMode(BUTTON_PIN, INPUT);

  // 3. 初始化 WS2812 LED
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(20);
  FastLED.clear();
  FastLED.show();

  // 4. 读取保存的设置（设备名/间隔/幅度/周计划/WiFi）+ 访问密码
  settingsLoad();
  adminLoad();

  // 5. 启动蓝牙鼠标（用设置里的设备名）
  bleMouse.begin(g.btName, "RAPOO", 100);

  // 6. 随机种子：硬件真随机数，不占用 GPIO32（留给 Grove I2C）
  randomSeed(esp_random());
  nextMoveInterval = random((long)g.moveMinSec * 1000, (long)g.moveMaxSec * 1000 + 1);

  // 7. 自动检测 Unit ENV 温湿度传感器（I2C: G26/G32）
  envBegin();

  // 8. 启动 WiFi（先试连已保存网络，否则开 AP 配网）+ Web 服务
  netBegin();
  webBegin();

  Serial.println("MouseJigger ready");
}

// 长按按键 5 秒：清除蓝牙配对 + WiFi 设置并重启
void clearPairingAndRestart() {
  Serial.println("Resetting pairing & WiFi...");
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(1000);
  NimbleMouse::clearBonds();   // 删除全部蓝牙配对
  settingsClearWifi();         // 清除 WiFi 凭据
  adminClearPass();            // 清除访问密码
  ESP.restart();
}

void loop() {
  unsigned long currentMillis = millis();

  // ==================== 按键逻辑：长按5秒清除配对+WiFi ====================
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!isButtonPressed) {
      isButtonPressed = true;
      buttonPressStartTime = currentMillis;
    } else if (currentMillis - buttonPressStartTime > 5000) {
      clearPairingAndRestart();
    }
  } else {
    isButtonPressed = false;
  }

  // ==================== 网络 / Web 服务 ====================
  netLoop();   // AP 模式下处理强制门户 DNS
  webLoop();   // 处理 Web 请求

  // 设备名变更请求的重启
  if (g_needReboot) { delay(400); ESP.restart(); }

  // ==================== 温湿度定时刷新（每 60 秒）====================
  if (g_env.present && currentMillis - lastEnvRead > 60000) {
    envRead();
    lastEnvRead = currentMillis;
  }

  // ==================== 蓝牙状态（供页面显示）====================
  g_bleConnected = bleMouse.isConnected();
  g_bleBond = NimbleMouse::bondCount();

  // ==================== 状态机与 LED 指示 ====================
  if (g_bleConnected) {
    bool allow = netScheduleAllowsNow();
    // 已连接：计划允许时段=绿灯；计划暂停时段=黄灯
    leds[0] = allow ? CRGB::Green : CRGB::Yellow;
    FastLED.show();

    // ==================== 鼠标移动逻辑 ====================
    if (allow && currentMillis - lastMoveTime > nextMoveInterval) {
      // 随机生成移动幅度（settings 的上下限）
      int lo = g.ampMin, hi = g.ampMax;
      int moveX = (hi > lo) ? random(lo, hi + 1) : lo;
      int moveY = (hi > lo) ? random(lo, hi + 1) : lo;
      // 随机方向
      if (random(0, 2) == 0) moveX = -moveX;
      if (random(0, 2) == 0) moveY = -moveY;

      bleMouse.move(moveX, moveY);
      Serial.printf("Mouse moved: X:%d, Y:%d\n", moveX, moveY);

      // 记录时间并按 settings 间隔生成下一次随机延时
      lastMoveTime = currentMillis;
      long ilo = (long)g.moveMinSec * 1000, ihi = (long)g.moveMaxSec * 1000;
      nextMoveInterval = (ihi > ilo) ? random(ilo, ihi) : ilo;
    }

  } else {
    // 未连接状态，判断是否曾经配对过
    if (g_bleBond > 0) {
      // 已配对但未连接 -> 蓝灯常亮
      leds[0] = CRGB::Blue;
      FastLED.show();
    } else {
      // 没有任何配对 -> 蓝灯慢闪等待配对
      if (currentMillis - lastBlinkTime > 1000) {
        isLedOn = !isLedOn;
        leds[0] = isLedOn ? CRGB::Blue : CRGB::Black;
        FastLED.show();
        lastBlinkTime = currentMillis;
      }
    }
  }

  delay(5);
}
