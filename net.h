// net.h — WiFi 配网 / Captive Portal / mDNS / NTP 时间 / 周计划判断
//
// 启动流程：若已保存 WiFi → 尝试以 STA 连接；连上则启用 mDNS+NTP；
// 连不上或未配置 → 开无密码 AP "MouseJigger" + 强制门户(DNS 全劫持)。
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include "settings.h"

#define MJ_AP_SSID   "MouseJigger"
#define MJ_HOSTNAME  "mousejigger"
#define MJ_TZ        "JST-9"   // 东京时间 UTC+9（POSIX TZ：东 9 区写作 JST-9）

bool g_staConnected = false;   // 已连上目标 WiFi
bool g_apMode       = false;   // 处于 AP 配网模式

static DNSServer mjDns;

inline void netStartMdns() {
  if (MDNS.begin(MJ_HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

// 联网后启动 NTP（东京时间）
inline void netStartNtp() {
  configTzTime(MJ_TZ, "ntp.nict.jp", "pool.ntp.org", "time.google.com");
}

// 尝试以 STA 连接已保存 WiFi，超时返回 false
inline bool netTryConnectSaved(uint32_t timeoutMs = 15000) {
  if (g.wifiSsid[0] == 0) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MJ_HOSTNAME);
  WiFi.begin(g.wifiSsid, g.wifiPass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(200);
  return WiFi.status() == WL_CONNECTED;
}

inline void netStartStaServices() {
  g_staConnected = true;
  g_apMode = false;
  netStartMdns();
  netStartNtp();
  Serial.printf("WiFi STA connected: %s  http://%s.local/ (%s)\n",
                g.wifiSsid, MJ_HOSTNAME, WiFi.localIP().toString().c_str());
}

// 开 AP 配网模式（AP_STA：保留 STA 以便扫描周边 WiFi）
inline void netStartApMode() {
  g_apMode = true;
  g_staConnected = false;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(MJ_AP_SSID);        // 无密码
  delay(100);
  mjDns.start(53, "*", WiFi.softAPIP());  // 所有域名指向自己 → 触发系统强制门户
  netStartMdns();
  Serial.printf("WiFi AP mode: SSID=%s  http://%s/\n",
                MJ_AP_SSID, WiFi.softAPIP().toString().c_str());
}

inline void netBegin() {
  if (netTryConnectSaved()) netStartStaServices();
  else netStartApMode();
}

inline void netLoop() {
  if (g_apMode) mjDns.processNextRequest();
}

// 页面「连接此 WiFi」：保存凭据并在线尝试连接，返回是否成功
inline bool netConnectTo(const char* ssid, const char* pass) {
  strncpy(g.wifiSsid, ssid, sizeof(g.wifiSsid)); g.wifiSsid[sizeof(g.wifiSsid)-1] = 0;
  strncpy(g.wifiPass, pass, sizeof(g.wifiPass)); g.wifiPass[sizeof(g.wifiPass)-1] = 0;
  WiFi.begin(g.wifiSsid, g.wifiPass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    settingsSave();
    g_staConnected = true;
    netStartMdns();
    netStartNtp();
    Serial.printf("WiFi connected via portal: %s (%s)\n",
                  g.wifiSsid, WiFi.localIP().toString().c_str());
    return true;
  }
  return false;
}

// ── 时间 / 周计划 ────────────────────────────────────────────────────────────

// 是否已取得有效时间（NTP 同步成功）
inline bool netTimeValid() {
  struct tm t;
  if (!getLocalTime(&t, 5)) return false;
  return t.tm_year > 120;  // >2020 年视为有效
}

// 当前东京时间字符串，例 "2026/06/25 19:47:46"；无效时返回空串
inline String netNowString() {
  struct tm t;
  if (!getLocalTime(&t, 5)) return String("");
  char buf[24];
  strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &t);
  return String(buf);
}

// 按周计划判断「此刻是否允许随机移动」。无有效时间时默认允许（退化为始终工作）。
inline bool netScheduleAllowsNow() {
  struct tm t;
  if (!getLocalTime(&t, 5) || t.tm_year <= 120) return true;
  int idx = t.tm_hour * 2 + (t.tm_min >= 30 ? 1 : 0);   // 0..47
  bool weekday = (t.tm_wday >= 1 && t.tm_wday <= 5);     // 周一~周五
  uint64_t mask = weekday ? g.schedWeekday : g.schedWeekend;
  return (mask >> idx) & 1ULL;
}
