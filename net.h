// net.h — WiFi（AP 常开 + STA 并存）/ Captive Portal / mDNS / NTP / 周计划
//
// 设计：无密码热点 "MouseJigger" 始终开启（即使已连上路由），方便随时连回设备；
// 同时若已保存路由凭据则连接之(STA)。两者并存：内网用 mousejigger.local 访问，
// 找不到时连回热点访问。
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

bool g_staConnected = false;   // STA 是否曾连上（用于只启动一次 NTP）

static DNSServer mjDns;

inline void netStartMdns() {
  if (MDNS.begin(MJ_HOSTNAME)) MDNS.addService("http", "tcp", 80);
}
inline void netStartNtp() {
  configTzTime(MJ_TZ, "ntp.nict.jp", "pool.ntp.org", "time.google.com");
}

// 连接已保存的路由（STA），超时返回结果。AP 保持开启不受影响。
inline bool netStaConnect(uint32_t timeoutMs = 15000) {
  if (g.wifiSsid[0] == 0) return false;
  WiFi.begin(g.wifiSsid, g.wifiPass);
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < timeoutMs) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    if (!g_staConnected) { g_staConnected = true; netStartNtp(); }
    Serial.printf("WiFi STA connected: %s (%s)\n", g.wifiSsid, WiFi.localIP().toString().c_str());
    return true;
  }
  return false;
}

// 开机：AP 始终开（便于随时连回）+ 若已配置则同时连 STA
inline void netBegin() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(MJ_HOSTNAME);
  WiFi.softAP(MJ_AP_SSID);                 // 无密码热点，始终开启
  delay(100);
  mjDns.start(53, "*", WiFi.softAPIP());   // 强制门户：所有域名指向自己
  netStartMdns();
  Serial.printf("WiFi AP up: %s  http://%s/\n", MJ_AP_SSID, WiFi.softAPIP().toString().c_str());
  netStaConnect();                         // 有保存的网络就连
}

inline void netLoop() { mjDns.processNextRequest(); }

// 页面「连接此 WiFi」：保存凭据并在线尝试连接
inline bool netConnectTo(const char* ssid, const char* pass) {
  strncpy(g.wifiSsid, ssid, sizeof(g.wifiSsid)); g.wifiSsid[sizeof(g.wifiSsid)-1] = 0;
  strncpy(g.wifiPass, pass, sizeof(g.wifiPass)); g.wifiPass[sizeof(g.wifiPass)-1] = 0;
  if (netStaConnect(12000)) { settingsSave(); return true; }
  return false;
}

// STA 当前是否已连上
inline bool netStaUp() { return WiFi.status() == WL_CONNECTED; }

// ── 时间 / 周计划 ────────────────────────────────────────────────────────────

// 是否已取得有效时间（NTP 同步成功）
inline bool netTimeValid() {
  struct tm t;
  if (!getLocalTime(&t, 5)) return false;
  return t.tm_year > 120;  // >2020 年视为有效
}

// 当前东京时间字符串，例 "2026/06/25 20:44:13"；无效时返回空串
inline String netNowString() {
  struct tm t;
  if (!getLocalTime(&t, 5)) return String("");
  char buf[24];
  strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &t);
  return String(buf);
}

// 按周计划判断「此刻是否允许随机移动」。无有效时间时默认允许。
// 每天 24 格（每格 1 小时），idx = 当前小时(0..23)。
inline bool netScheduleAllowsNow() {
  struct tm t;
  if (!getLocalTime(&t, 5) || t.tm_year <= 120) return true;
  int idx = t.tm_hour;                                 // 0..23
  bool weekday = (t.tm_wday >= 1 && t.tm_wday <= 5);   // 周一~周五
  uint64_t mask = weekday ? g.schedWeekday : g.schedWeekend;
  return (mask >> idx) & 1ULL;
}
