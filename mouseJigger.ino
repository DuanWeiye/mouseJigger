#include <Arduino.h>
#include <BleMouse.h>
#include <FastLED.h>
#include <nvs_flash.h>
#include <esp_gap_ble_api.h>

// --- 引脚定义 ---
#define LED_PIN     27
#define BUTTON_PIN  39
#define NUM_LEDS    1

// --- FastLED 设置 ---
CRGB leds[NUM_LEDS];

// --- 蓝牙鼠标实例化 ---
// 参数: 设备名, 制造商, 电池电量
BleMouse bleMouse("RAPOO BT MOUSE", "RAPOO", 100);

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

  // 4. 启动蓝牙鼠标
  bleMouse.begin();
  
  // 5. 初始化随机种子
  randomSeed(analogRead(32)); // 读取一个空引脚作为随机数种子
  
  // 初始化首次移动的随机时间 (5-8分钟)
  // 5分钟 = 300,000 ms, 8分钟 = 480,000 ms
  nextMoveInterval = random(300000, 480000); 
}

// 检查是否有已保存的配对(Bonded)设备
bool hasBondedDevice() {
  return esp_ble_get_bond_device_num() > 0;
}

// 清除配对信息并重启
void clearPairingAndRestart() {
  Serial.println("Clearing pairing information...");
  leds[0] = CRGB::Red; // 重置时闪烁红灯提示
  FastLED.show();
  delay(1000);
  
  // 擦除 NVS 分区（保存了蓝牙配对信息）
  nvs_flash_erase();
  nvs_flash_init();
  
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
    if (hasBondedDevice()) {
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