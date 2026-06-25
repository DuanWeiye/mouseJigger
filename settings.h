// settings.h — 用户设置（存 NVS，掉电不丢）
//
// 包含蓝牙设备名、随机移动的间隔/幅度、两行周计划（工作日/周末各 48 个半小时
// 格子）、以及已保存的 WiFi 凭据。整个结构按字节存入 NVS namespace "mj"。
#pragma once
#include <Arduino.h>
#include <Preferences.h>

// 周计划：每天分 24 格（每格 1 小时，0 点=bit0 ... 23 点=bit23）。
// 低 24 位有效，bit=1 表示该小时「允许随机移动」。
static const uint64_t MJ_SCHED_ALL = 0xFFFFFFULL;  // 24 位全 1

struct Settings {
  char     btName[32];     // 蓝牙设备名
  uint16_t moveMinSec;     // 随机移动间隔下限（秒）
  uint16_t moveMaxSec;     // 随机移动间隔上限（秒）
  uint8_t  ampMin;         // 随机移动幅度下限（像素）
  uint8_t  ampMax;         // 随机移动幅度上限（像素）
  uint64_t schedWeekday;   // 工作日(周一~周五) 24 小时位图(每位1小时)
  uint64_t schedWeekend;   // 周末(周六/周日) 24 小时位图(每位1小时)
  char     wifiSsid[33];   // 目标 WiFi 名（空=未配置）
  char     wifiPass[65];   // 目标 WiFi 密码
};

Settings g;  // 全局设置（本工程仅一个编译单元，定义放这里）

static Preferences mjPrefs;

inline void settingsDefaults(Settings& s) {
  strncpy(s.btName, "RAPOO BT MOUSE", sizeof(s.btName));
  s.btName[sizeof(s.btName) - 1] = 0;
  s.moveMinSec   = 300;   // 5 分钟
  s.moveMaxSec   = 480;   // 8 分钟
  s.ampMin       = 2;
  s.ampMax       = 5;
  s.schedWeekday = MJ_SCHED_ALL;
  s.schedWeekend = MJ_SCHED_ALL;
  s.wifiSsid[0]  = 0;
  s.wifiPass[0]  = 0;
}

// 校验/收敛非法值，防止页面传入越界数据
inline void settingsSanitize(Settings& s) {
  s.btName[sizeof(s.btName) - 1] = 0;
  if (s.btName[0] == 0) strncpy(s.btName, "RAPOO BT MOUSE", sizeof(s.btName));
  if (s.moveMinSec < 5)   s.moveMinSec = 5;       // 最少 5 秒，避免过于频繁
  if (s.moveMaxSec < s.moveMinSec) s.moveMaxSec = s.moveMinSec;
  if (s.ampMin < 1)  s.ampMin = 1;
  if (s.ampMax > 100) s.ampMax = 100;             // HID 相对位移上限 127
  if (s.ampMax < s.ampMin) s.ampMax = s.ampMin;
  s.schedWeekday &= MJ_SCHED_ALL;
  s.schedWeekend &= MJ_SCHED_ALL;
}

inline void settingsLoad() {
  mjPrefs.begin("mj", false);
  size_t n = mjPrefs.getBytesLength("cfg");
  if (n == sizeof(Settings)) {
    mjPrefs.getBytes("cfg", &g, sizeof(Settings));
    settingsSanitize(g);
  } else {
    settingsDefaults(g);
  }
  mjPrefs.end();
}

inline void settingsSave() {
  settingsSanitize(g);
  mjPrefs.begin("mj", false);
  mjPrefs.putBytes("cfg", &g, sizeof(Settings));
  mjPrefs.end();
}

// 只清空 WiFi 凭据（长按重置 / 页面「重置 WiFi」用）
inline void settingsClearWifi() {
  g.wifiSsid[0] = 0;
  g.wifiPass[0] = 0;
  settingsSave();
}

// ── 配置页访问密码 ──────────────────────────────────────────────────────────
// 单独存一个 NVS 键，不放进 Settings 结构，避免结构体大小变化导致旧设置失效。
String g_adminPass = "";  // 空 = 不需要密码

inline void adminLoad() {
  mjPrefs.begin("mj", true);
  g_adminPass = mjPrefs.getString("pass", "");
  mjPrefs.end();
}
inline void adminSetPass(const String& p) {
  g_adminPass = p;
  mjPrefs.begin("mj", false);
  mjPrefs.putString("pass", p);
  mjPrefs.end();
}
inline void adminClearPass() {
  g_adminPass = "";
  mjPrefs.begin("mj", false);
  mjPrefs.remove("pass");
  mjPrefs.end();
}
