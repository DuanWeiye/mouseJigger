// env.h — M5Stack Unit ENV 自动检测与温湿度/气压读取
//
// 通过 ATOM Lite 的 Grove 口 I2C(SDA=26, SCL=32) 扫描总线，自动识别接入的
// Unit ENV 系列传感器并读数：
//   温湿度：SHT30(ENV/II/III, 0x44)、SHT40(ENV IV, 0x44)、DHT12(初代 ENV, 0x5C)
//   气压：  BMP280(初代 ENV / ENV II / ENV IV, 0x76 或 0x77)
// 未接传感器时对应字段隐藏。
#pragma once
#include <Arduino.h>
#include <Wire.h>

#define MJ_I2C_SDA 26   // ATOM Lite Grove SDA
#define MJ_I2C_SCL 32   // ATOM Lite Grove SCL

struct EnvData {
  bool  present = false;     // 是否检测到温湿度传感器
  char  model[20] = "";      // 传感器型号字符串
  float tempC = 0;           // 温度(℃)
  float hum   = 0;           // 相对湿度(%)
  bool  hasPressure = false; // 是否带气压传感器
  float pressure = 0;        // 气压(hPa)
};
EnvData g_env;

enum { ENV_NONE = 0, ENV_SHT30, ENV_SHT40, ENV_DHT12 };
static int s_envType = ENV_NONE;

static bool envI2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// ── 温湿度：SHT3x/SHT4x + DHT12 ──────────────────────────────────────────────
static uint8_t envCrc8(const uint8_t* d, int n) {  // SHT 多项式 0x31，初值 0xFF
  uint8_t crc = 0xFF;
  for (int i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  }
  return crc;
}

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
  if (((d[0] + d[1] + d[2] + d[3]) & 0xFF) != d[4]) return false;
  g_env.hum   = d[0] + d[1] * 0.1f;
  g_env.tempC = (d[2] & 0x7F) + (d[3] & 0x0F) * 0.1f;
  if (d[3] & 0x80) g_env.tempC = -g_env.tempC;
  return true;
}

// ── 气压：BMP280 ─────────────────────────────────────────────────────────────
static uint8_t s_bmpAddr = 0;
static struct { uint16_t T1; int16_t T2, T3; uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9; } s_bmp;

static void bmpWrite(uint8_t a, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(a); Wire.write(reg); Wire.write(val); Wire.endTransmission();
}

static bool bmpReadCalib(uint8_t a) {
  Wire.beginTransmission(a); Wire.write(0x88);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)a, 24) != 24) return false;
  uint8_t d[24];
  for (int i = 0; i < 24; i++) d[i] = Wire.read();
  s_bmp.T1 = d[0] | (d[1] << 8);   s_bmp.T2 = d[2] | (d[3] << 8);   s_bmp.T3 = d[4] | (d[5] << 8);
  s_bmp.P1 = d[6] | (d[7] << 8);   s_bmp.P2 = d[8] | (d[9] << 8);   s_bmp.P3 = d[10] | (d[11] << 8);
  s_bmp.P4 = d[12] | (d[13] << 8); s_bmp.P5 = d[14] | (d[15] << 8); s_bmp.P6 = d[16] | (d[17] << 8);
  s_bmp.P7 = d[18] | (d[19] << 8); s_bmp.P8 = d[20] | (d[21] << 8); s_bmp.P9 = d[22] | (d[23] << 8);
  return true;
}

static bool bmpDetect() {
  for (uint8_t a = 0x76; a <= 0x77; a++) {
    if (!envI2cPresent(a)) continue;
    Wire.beginTransmission(a); Wire.write(0xD0);
    if (Wire.endTransmission() != 0) continue;
    if (Wire.requestFrom((int)a, 1) != 1) continue;
    uint8_t id = Wire.read();
    if (id == 0x58 || id == 0x60) {       // 0x58=BMP280, 0x60=BME280(按气压用)
      if (bmpReadCalib(a)) {
        bmpWrite(a, 0xF4, 0x27);          // osrs_t x1, osrs_p x1, normal mode
        bmpWrite(a, 0xF5, 0xA0);          // standby 1000ms, filter off
        s_bmpAddr = a;
        return true;
      }
    }
  }
  return false;
}

static void bmpRead() {
  if (!s_bmpAddr) return;
  Wire.beginTransmission(s_bmpAddr); Wire.write(0xF7);
  if (Wire.endTransmission() != 0) return;
  if (Wire.requestFrom((int)s_bmpAddr, 6) != 6) return;
  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();
  int32_t adc_P = ((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | (d[2] >> 4);
  int32_t adc_T = ((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | (d[5] >> 4);
  // Bosch 浮点补偿（datasheet）
  double v1 = (((double)adc_T) / 16384.0 - ((double)s_bmp.T1) / 1024.0) * (double)s_bmp.T2;
  double v2 = ((((double)adc_T) / 131072.0 - ((double)s_bmp.T1) / 8192.0) *
               (((double)adc_T) / 131072.0 - ((double)s_bmp.T1) / 8192.0)) * (double)s_bmp.T3;
  double t_fine = v1 + v2;
  double p1 = t_fine / 2.0 - 64000.0;
  double p2 = p1 * p1 * (double)s_bmp.P6 / 32768.0;
  p2 = p2 + p1 * (double)s_bmp.P5 * 2.0;
  p2 = p2 / 4.0 + (double)s_bmp.P4 * 65536.0;
  p1 = ((double)s_bmp.P3 * p1 * p1 / 524288.0 + (double)s_bmp.P2 * p1) / 524288.0;
  p1 = (1.0 + p1 / 32768.0) * (double)s_bmp.P1;
  if (p1 == 0) return;
  double p = 1048576.0 - (double)adc_P;
  p = (p - p2 / 4096.0) * 6250.0 / p1;
  p1 = (double)s_bmp.P9 * p * p / 2147483648.0;
  p2 = p * (double)s_bmp.P8 / 32768.0;
  p = p + (p1 + p2 + (double)s_bmp.P7) / 16.0;
  g_env.pressure = p / 100.0;   // Pa -> hPa
}

// ── 对外接口 ─────────────────────────────────────────────────────────────────
inline void envBegin() {
  Wire.begin(MJ_I2C_SDA, MJ_I2C_SCL);
  Wire.setClock(100000);
  delay(20);

  // 温湿度
  if (envI2cPresent(0x44)) {
    uint8_t c40[1] = {0xFD};
    uint8_t c30[2] = {0x24, 0x00};
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

  // 气压
  g_env.hasPressure = bmpDetect();
  if (g_env.hasPressure) {
    bmpRead();
    if (g_env.present) strncat(g_env.model, "+BMP280", sizeof(g_env.model) - strlen(g_env.model) - 1);
    else strncpy(g_env.model, "BMP280", sizeof(g_env.model));
  }

  if (g_env.present || g_env.hasPressure)
    Serial.printf("ENV sensor: %s  %.1fC %.1f%% %.0fhPa\n",
                  g_env.model, g_env.tempC, g_env.hum, g_env.pressure);
  else
    Serial.println("ENV sensor: none");
}

// 读取一次温湿度+气压（每分钟调用）；失败保留上次值
inline void envRead() {
  switch (s_envType) {
    case ENV_SHT40: { uint8_t c[1] = {0xFD};       envReadSht(0x44, c, 1, 12); break; }
    case ENV_SHT30: { uint8_t c[2] = {0x24, 0x00}; envReadSht(0x44, c, 2, 18); break; }
    case ENV_DHT12: envReadDht12(); break;
    default: break;
  }
  if (s_bmpAddr) bmpRead();
}

// 是否检测到任意 ENV 传感器（温湿度或气压）
inline bool envAny() { return g_env.present || g_env.hasPressure; }
