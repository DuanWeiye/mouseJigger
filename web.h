// web.h — Web 服务器：配置页面(PROGMEM) + JSON API
//
// 同步 WebServer，loop() 里 handleClient()。AP 模式下未知路径返回首页以触发
// 系统强制门户。所有页面资源内嵌固件，烧录无需上传文件系统。
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "settings.h"
#include "net.h"
#include "env.h"

// 蓝牙状态由主程序(mouseJigger.ino)每 loop 更新，这里只读
extern bool g_bleConnected;
extern int  g_bleBond;
// 设备名变更需要重启蓝牙才生效，置位后由主循环择机重启
extern bool g_needReboot;

WebServer mjServer(80);

// ── 工具：JSON 字符串转义 ────────────────────────────────────────────────────
static String mjJsonEsc(const String& s) {
  String o; o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else if (c == '\r') {}
    else o += c;
  }
  return o;
}

// ── 配置页面 ────────────────────────────────────────────────────────────────
static const char MJ_INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MouseJigger</title>
<style>
*{box-sizing:border-box}body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;margin:0;background:#f3f4f6;color:#1f2937}
.wrap{max-width:760px;margin:0 auto;padding:16px}
h1{font-size:20px;margin:8px 0}h2{font-size:15px;margin:0 0 10px;color:#374151}
.card{background:#fff;border-radius:12px;padding:16px;margin:12px 0;box-shadow:0 1px 3px rgba(0,0,0,.08)}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:8px 0}
label{font-size:13px;color:#4b5563;min-width:96px}
input[type=text],input[type=password],input[type=number]{padding:8px;border:1px solid #d1d5db;border-radius:8px;font-size:14px;flex:1;min-width:120px}
input[type=number]{flex:none;width:96px}
button{padding:8px 14px;border:0;border-radius:8px;background:#2563eb;color:#fff;font-size:14px;cursor:pointer}
button.sec{background:#6b7280}button.warn{background:#dc2626}button.mini{padding:4px 8px;font-size:12px;background:#9ca3af}
.status{font-size:13px;line-height:1.7}.status b{color:#111827}
.badge{display:inline-block;padding:2px 8px;border-radius:999px;font-size:12px;color:#fff}
.on{background:#16a34a}.off{background:#9ca3af}
ul.aps{list-style:none;padding:0;margin:8px 0;max-height:170px;overflow:auto}
ul.aps li{padding:8px;border:1px solid #e5e7eb;border-radius:8px;margin:4px 0;cursor:pointer;display:flex;justify-content:space-between}
ul.aps li:hover{background:#eff6ff}
.sched{overflow-x:auto}
.grid{display:grid;grid-template-columns:64px repeat(48,14px);gap:1px;align-items:center;font-size:10px}
.grid .hh{grid-column:span 2;text-align:left;color:#9ca3af}
.cell{width:14px;height:18px;border:1px solid #e5e7eb;border-radius:2px;cursor:pointer;background:#fff}
.cell.on{background:#2563eb;border-color:#2563eb}
.muted{color:#9ca3af;font-size:12px}.ok{color:#16a34a}.err{color:#dc2626}
.hide{display:none}
</style></head><body><div class="wrap">
<h1>🖱️ MouseJigger 配置</h1>

<div class="card"><h2>状态</h2><div class="status" id="status">加载中…</div></div>

<div class="card"><h2>WiFi</h2>
<div class="row"><button onclick="scan()">扫描 WiFi</button><span class="muted" id="scanmsg"></span></div>
<ul class="aps" id="aps"></ul>
<div class="row"><label>名称(SSID)</label><input type="text" id="ssid" placeholder="选择上方或手动输入"></div>
<div class="row"><label>密码</label><input type="password" id="pass" placeholder="开放网络留空"></div>
<div class="row"><button onclick="connect()">连接此 WiFi</button>
<button class="warn" onclick="resetWifi()">重置 WiFi</button><span id="wifimsg" class="muted"></span></div>
</div>

<div class="card"><h2>设置</h2>
<div class="row"><label>蓝牙设备名</label><input type="text" id="btName" maxlength="31"></div>
<div class="row"><label>移动间隔(秒)</label>最短<input type="number" id="moveMin" min="5">最长<input type="number" id="moveMax" min="5"></div>
<div class="row"><label>移动幅度(像素)</label>最小<input type="number" id="ampMin" min="1" max="100">最大<input type="number" id="ampMax" min="1" max="100"></div>

<h2 style="margin-top:14px">周计划（勾选=该半小时允许移动）</h2>
<p class="muted">需联网取得时间后生效；未联网时始终允许移动。</p>
<div class="sched">
  <div class="row" style="margin:2px 0"><label>工作日(一~五)</label>
    <button class="mini" onclick="fill('wd',1)">全天</button>
    <button class="mini" onclick="fill('wd',0)">清空</button>
    <button class="mini" onclick="inv('wd')">反选</button></div>
  <div class="grid" id="wd"></div>
  <div class="row" style="margin:8px 0 2px"><label>周末(六/日)</label>
    <button class="mini" onclick="fill('we',1)">全天</button>
    <button class="mini" onclick="fill('we',0)">清空</button>
    <button class="mini" onclick="inv('we')">反选</button></div>
  <div class="grid" id="we"></div>
</div>
<div class="row" style="margin-top:14px"><button onclick="saveSettings()">保存设置</button>
<button class="sec" onclick="restoreDefaults()">恢复默认</button><span id="setmsg" class="muted"></span></div>
</div>

<div class="card hide" id="envcard"><h2>温湿度</h2>
<div class="status" id="env"></div></div>

<p class="muted" style="text-align:center">MouseJigger · ESP32 BLE 鼠标随机移动器</p>
</div>
<script>
const $=id=>document.getElementById(id);
function hourScale(){let s='';for(let h=0;h<24;h++)s+=`<div class="hh">${h}</div>`;return s;}
function buildGrid(id){let g=$(id);g.innerHTML='<div></div>'+hourScale();
  for(let i=0;i<48;i++){let c=document.createElement('div');c.className='cell';c.dataset.i=i;
    c.onclick=()=>c.classList.toggle('on');g.appendChild(c);}}
function setGrid(id,hex){let v=BigInt('0x'+(hex||'0'));
  $(id).querySelectorAll('.cell').forEach(c=>{let i=BigInt(c.dataset.i);
    c.classList.toggle('on',((v>>i)&1n)===1n);});}
function getGrid(id){let v=0n;$(id).querySelectorAll('.cell').forEach(c=>{
  if(c.classList.contains('on'))v|=(1n<<BigInt(c.dataset.i));});return v.toString(16).padStart(12,'0');}
function fill(id,on){$(id).querySelectorAll('.cell').forEach(c=>c.classList.toggle('on',!!on));}
function inv(id){$(id).querySelectorAll('.cell').forEach(c=>c.classList.toggle('on'));}

async function loadStatus(){let r=await fetch('/api/status');let s=await r.json();
  let ble=s.ble?'<span class="badge on">已连接</span>':'<span class="badge off">未连接</span>';
  $('status').innerHTML=`蓝牙：${ble}（已配对 ${s.bond}）<br>`+
    `网络：<b>${s.mode}</b>　SSID：<b>${s.ssid||'-'}</b><br>`+
    `地址：<b>${s.ip}</b>　<span class="muted">http://${s.host}/</span><br>`+
    `时间：<b>${s.time||'未同步'}</b>`;}
async function loadEnv(){let r=await fetch('/api/env');let e=await r.json();
  if(e.present){$('envcard').classList.remove('hide');
    $('env').innerHTML=`型号：<b>${e.model}</b>　🌡️ <b>${e.temp.toFixed(1)}℃</b>　💧 <b>${e.hum.toFixed(1)}%</b>`;}
  else $('envcard').classList.add('hide');}
async function loadSettings(){let r=await fetch('/api/settings');let s=await r.json();
  $('btName').value=s.btName;$('moveMin').value=s.moveMin;$('moveMax').value=s.moveMax;
  $('ampMin').value=s.ampMin;$('ampMax').value=s.ampMax;setGrid('wd',s.wd);setGrid('we',s.we);}
async function scan(){$('scanmsg').textContent='扫描中…';
  let r=await fetch('/api/scan');let a=await r.json();$('scanmsg').textContent='';
  $('aps').innerHTML=a.map(x=>`<li onclick="document.getElementById('ssid').value='${x.ssid.replace(/'/g,"")}'">`+
    `<span>${x.ssid||'(隐藏)'} ${x.enc?'🔒':''}</span><span class="muted">${x.rssi}dBm</span></li>`).join('')
    ||'<li class="muted">未发现网络</li>';}
async function connect(){let ssid=$('ssid').value.trim();if(!ssid){alert('请填写 WiFi 名称');return;}
  $('wifimsg').textContent='连接中（约 10 秒）…';$('wifimsg').className='muted';
  let b=new URLSearchParams({ssid,pass:$('pass').value});
  let r=await fetch('/api/connect',{method:'POST',body:b});let j=await r.json();
  if(j.ok){$('wifimsg').innerHTML=`<span class="ok">已连接！可改用 http://${j.host}/ 或 ${j.ip} 访问</span>`;}
  else{$('wifimsg').innerHTML='<span class="err">连接失败，请检查密码后重试</span>';}
  loadStatus();}
async function resetWifi(){if(!confirm('确定清除已保存的 WiFi 并重启？'))return;
  await fetch('/api/resetwifi',{method:'POST'});
  $('wifimsg').innerHTML='<span class="muted">已重置，正在重启，请连接 MouseJigger 热点…</span>';}
async function saveSettings(){let b=new URLSearchParams({name:$('btName').value,
  moveMin:$('moveMin').value,moveMax:$('moveMax').value,ampMin:$('ampMin').value,ampMax:$('ampMax').value,
  wd:getGrid('wd'),we:getGrid('we')});
  let r=await fetch('/api/settings',{method:'POST',body:b});let j=await r.json();
  $('setmsg').innerHTML=j.reboot?'<span class="ok">已保存，设备名已改，正在重启…</span>':'<span class="ok">已保存 ✓</span>';}
async function restoreDefaults(){if(!confirm('恢复默认设置？(不影响 WiFi)'))return;
  let r=await fetch('/api/defaults',{method:'POST'});let j=await r.json();
  await loadSettings();$('setmsg').innerHTML=j.reboot?'<span class="ok">已恢复，正在重启…</span>':'<span class="ok">已恢复默认 ✓</span>';}

buildGrid('wd');buildGrid('we');loadStatus();loadSettings();loadEnv();
setInterval(loadStatus,5000);setInterval(loadEnv,60000);
</script></body></html>)HTML";

// ── API handlers ────────────────────────────────────────────────────────────
static void apiIndex() { mjServer.send_P(200, "text/html", MJ_INDEX_HTML); }

static void apiStatus() {
  String ip = g_apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String j = "{";
  j += "\"mode\":\"" + String(g_apMode ? "AP 配网" : "STA 已联网") + "\",";
  j += "\"ssid\":\"" + mjJsonEsc(g_apMode ? String(MJ_AP_SSID) : String(g.wifiSsid)) + "\",";
  j += "\"ip\":\"" + ip + "\",";
  j += "\"host\":\"" MJ_HOSTNAME ".local\",";
  j += "\"ble\":" + String(g_bleConnected ? "true" : "false") + ",";
  j += "\"bond\":" + String(g_bleBond) + ",";
  j += "\"time\":\"" + netNowString() + "\"}";
  mjServer.send(200, "application/json", j);
}

static void apiScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    j += "{\"ssid\":\"" + mjJsonEsc(WiFi.SSID(i)) + "\",";
    j += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    j += "\"enc\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
  }
  j += "]";
  WiFi.scanDelete();
  mjServer.send(200, "application/json", j);
}

static void apiConnect() {
  String ssid = mjServer.arg("ssid");
  String pass = mjServer.arg("pass");
  bool ok = netConnectTo(ssid.c_str(), pass.c_str());
  String j = "{\"ok\":" + String(ok ? "true" : "false");
  if (ok) j += ",\"ip\":\"" + WiFi.localIP().toString() + "\",\"host\":\"" MJ_HOSTNAME ".local\"";
  j += "}";
  mjServer.send(200, "application/json", j);
}

static void apiResetWifi() {
  mjServer.send(200, "application/json", "{\"ok\":true}");
  delay(300);
  settingsClearWifi();
  ESP.restart();
}

static void apiGetSettings() {
  char wd[16], we[16];
  snprintf(wd, sizeof(wd), "%012llx", (unsigned long long)g.schedWeekday);
  snprintf(we, sizeof(we), "%012llx", (unsigned long long)g.schedWeekend);
  String j = "{";
  j += "\"btName\":\"" + mjJsonEsc(g.btName) + "\",";
  j += "\"moveMin\":" + String(g.moveMinSec) + ",";
  j += "\"moveMax\":" + String(g.moveMaxSec) + ",";
  j += "\"ampMin\":" + String(g.ampMin) + ",";
  j += "\"ampMax\":" + String(g.ampMax) + ",";
  j += "\"wd\":\"" + String(wd) + "\",\"we\":\"" + String(we) + "\"}";
  mjServer.send(200, "application/json", j);
}

static void apiPostSettings() {
  String oldName = String(g.btName);
  strncpy(g.btName, mjServer.arg("name").c_str(), sizeof(g.btName)); g.btName[sizeof(g.btName)-1] = 0;
  g.moveMinSec = mjServer.arg("moveMin").toInt();
  g.moveMaxSec = mjServer.arg("moveMax").toInt();
  g.ampMin     = mjServer.arg("ampMin").toInt();
  g.ampMax     = mjServer.arg("ampMax").toInt();
  g.schedWeekday = strtoull(mjServer.arg("wd").c_str(), nullptr, 16);
  g.schedWeekend = strtoull(mjServer.arg("we").c_str(), nullptr, 16);
  settingsSave();
  bool reboot = (oldName != String(g.btName));   // 设备名变更需重启蓝牙
  mjServer.send(200, "application/json", String("{\"ok\":true,\"reboot\":") + (reboot ? "true" : "false") + "}");
  if (reboot) g_needReboot = true;
}

static void apiDefaults() {
  // 恢复默认但保留 WiFi 凭据
  char ssid[33], pass[65];
  strncpy(ssid, g.wifiSsid, sizeof(ssid)); strncpy(pass, g.wifiPass, sizeof(pass));
  String oldName = String(g.btName);
  settingsDefaults(g);
  strncpy(g.wifiSsid, ssid, sizeof(g.wifiSsid)); strncpy(g.wifiPass, pass, sizeof(g.wifiPass));
  settingsSave();
  bool reboot = (oldName != String(g.btName));
  mjServer.send(200, "application/json", String("{\"ok\":true,\"reboot\":") + (reboot ? "true" : "false") + "}");
  if (reboot) g_needReboot = true;
}

static void apiEnv() {
  String j = "{\"present\":" + String(g_env.present ? "true" : "false");
  if (g_env.present) {
    j += ",\"model\":\"" + String(g_env.model) + "\"";
    j += ",\"temp\":" + String(g_env.tempC, 1);
    j += ",\"hum\":" + String(g_env.hum, 1);
  }
  j += "}";
  mjServer.send(200, "application/json", j);
}

inline void webBegin() {
  mjServer.on("/", apiIndex);
  mjServer.on("/api/status", apiStatus);
  mjServer.on("/api/scan", apiScan);
  mjServer.on("/api/connect", HTTP_POST, apiConnect);
  mjServer.on("/api/resetwifi", HTTP_POST, apiResetWifi);
  mjServer.on("/api/settings", HTTP_GET, apiGetSettings);
  mjServer.on("/api/settings", HTTP_POST, apiPostSettings);
  mjServer.on("/api/defaults", HTTP_POST, apiDefaults);
  mjServer.on("/api/env", apiEnv);
  // 强制门户：未知路径一律返回首页，触发系统「需要登录」弹窗
  mjServer.onNotFound(apiIndex);
  mjServer.begin();
}

inline void webLoop() { mjServer.handleClient(); }
