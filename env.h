// env.h — M5Stack Unit ENV 自动检测与温湿度读取
//
// 通过 ATOM Lite 的 Grove 口 I2C(SDA=26, SCL=32) 扫描总线，自动识别接入的
// Unit ENV 系列温湿度传感器并读数：
//   - SHT30 (ENV / ENV II / ENV III, 地址 0x44)
//   - SHT40 (ENV IV, 地址 0x44)
//   - DHT12 (初代 ENV, 地址 0x5C)
// 未接传感器时 present=false，页面自动隐藏温湿度区。
#pragma once
#include <Arduino.h>
#include <Wire.h>

#define MJ_I2C_SDA 26   // ATOM Lite Grove SDA
#define MJ_I2C_SCL 32   // ATOM Lite Grove SCL

struct EnvData {
  bool  present = false;   // 是否检测到温湿度传感器
  char  model[12] = "";    // 传感器型号字符串
  float tempC = 0;         // 温度(℃)
  float hum   = 0;         // 相对湿度(%)
};
EnvData g_env;

enum { ENV_NONE = 0, ENV_SHT30, ENV_SHT40, ENV_DHT12 };
static int s_envType = ENV_NONE;

static bool envI2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// SHT3x/SHT4x 共用的 CRC8（多项式 0x31，初值 0xFF）
static uint8_t envCrc8(const uint8_t* d, int n) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  }
  return crc;
}

// 读 6 字节 SHT 数据并校验，换算温湿度（SHT30/SHT40 数据格式相同）
static bool envReadSht(uint8_t addr, const uint8_t* cmd, int cmdLen, int waitMs) {
  Wire.beginTransmission(addr);
  for (int i = 0; i < cmdLen; i++) Wire.write(cmd[i]);
  if (Wire.endTransmission() != 0) return false;
  delay(waitMs);
  if (Wire.requestFrom((int)addr, 6) != 6) return false;
  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();
  if (envCrc8(&d[0], 2) != d[2] || envCrc8(&d[3], 2) != d[5]) return false;
  uint16_t rawT = (d[0] << 8) | d[1];
  uint16_t rawH = (d[3] << 8) | d[4];
  g_env.tempC = -45.0f + 175.0f * rawT / 65535.0f;
  g_env.hum   = 100.0f * rawH / 65535.0f;
  return true;
}

static bool envReadDht12() {
  Wire.beginTransmission(0x5C);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(0x5C, 5) != 5) return false;
  uint8_t d[5];
  for (int i = 0; i < 5; i++) d[i] = Wire.read();
  if (((d[0] + d[1] + d[2] + d[3]) & 0xFF) != d[4]) return false;  // 校验和
  g_env.hum   = d[0] + d[1] * 0.1f;
  g_env.tempC = (d[2] & 0x7F) + (d[3] & 0x0F) * 0.1f;
  if (d[3] & 0x80) g_env.tempC = -g_env.tempC;  // 温度符号位
  return true;
}

// 启动时扫描 I2C，自动识别传感器型号
inline void envBegin() {
  Wire.begin(MJ_I2C_SDA, MJ_I2C_SCL);
  Wire.setClock(100000);
  delay(20);

  if (envI2cPresent(0x44)) {
    // 0x44 同时是 SHT30/SHT40 的地址：先试 SHT40(0xFD)，失败再试 SHT30
    uint8_t c40[1] = {0xFD};
    uint8_t c30[2] = {0x24, 0x00};  // 高重复性、无时钟拉伸
    if (envReadSht(0x44, c40, 1, 12)) {
      s_envType = ENV_SHT40; strncpy(g_env.model, "SHT40", sizeof(g_env.model));
    } else if (envReadSht(0x44, c30, 2, 18)) {
      s_envType = ENV_SHT30; strncpy(g_env.model, "SHT30", sizeof(g_env.model));
    }
  }
  if (s_envType == ENV_NONE && envI2cPresent(0x5C)) {
    if (envReadDht12()) { s_envType = ENV_DHT12; strncpy(g_env.model, "DHT12", sizeof(g_env.model)); }
  }
  g_env.present = (s_envType != ENV_NONE);
  if (g_env.present)
    Serial.printf("ENV sensor: %s  %.1fC %.1f%%\n", g_env.model, g_env.tempC, g_env.hum);
  else
    Serial.println("ENV sensor: none");
}

// 读取一次温湿度（每分钟调用）；失败保留上次值
inline void envRead() {
  switch (s_envType) {
    case ENV_SHT40: { uint8_t c[1] = {0xFD};       envReadSht(0x44, c, 1, 12); break; }
    case ENV_SHT30: { uint8_t c[2] = {0x24, 0x00}; envReadSht(0x44, c, 2, 18); break; }
    case ENV_DHT12: envReadDht12(); break;
    default: break;
  }
}
