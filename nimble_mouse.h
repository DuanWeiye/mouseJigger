// nimble_mouse.h — 基于 NimBLE-Arduino 的轻量蓝牙 HID 鼠标
//
// 用 NimBLE 协议栈自实现一个标准相对鼠标，替代 Bluedroid 版 ESP32-BLE-Mouse，
// 目的是大幅降低 RAM 占用，为 WiFi / Web 服务腾出空间（两者需在 ESP32 上共存）。
//
// 依赖库：NimBLE-Arduino（Arduino 库管理器搜索 "NimBLE-Arduino" 安装）。
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// 标准相对鼠标报文：Report ID 1，4 字节 = 按键, X, Y, 滚轮
static const uint8_t MJ_MOUSE_REPORT_MAP[] = {
  0x05, 0x01,  // Usage Page (Generic Desktop)
  0x09, 0x02,  // Usage (Mouse)
  0xA1, 0x01,  // Collection (Application)
  0x85, 0x01,  //   Report ID (1)
  0x09, 0x01,  //   Usage (Pointer)
  0xA1, 0x00,  //   Collection (Physical)
  0x05, 0x09,  //     Usage Page (Button)
  0x19, 0x01,  //     Usage Minimum (Button 1)
  0x29, 0x03,  //     Usage Maximum (Button 3)
  0x15, 0x00,  //     Logical Minimum (0)
  0x25, 0x01,  //     Logical Maximum (1)
  0x95, 0x03,  //     Report Count (3)
  0x75, 0x01,  //     Report Size (1)
  0x81, 0x02,  //     Input (Data,Var,Abs) - 3 个按键位
  0x95, 0x01,  //     Report Count (1)
  0x75, 0x05,  //     Report Size (5)
  0x81, 0x03,  //     Input (Const) - 5 位填充
  0x05, 0x01,  //     Usage Page (Generic Desktop)
  0x09, 0x30,  //     Usage (X)
  0x09, 0x31,  //     Usage (Y)
  0x09, 0x38,  //     Usage (Wheel)
  0x15, 0x81,  //     Logical Minimum (-127)
  0x25, 0x7F,  //     Logical Maximum (127)
  0x75, 0x08,  //     Report Size (8)
  0x95, 0x03,  //     Report Count (3)
  0x81, 0x06,  //     Input (Data,Var,Rel) - X,Y,Wheel
  0xC0,        //   End Collection
  0xC0         // End Collection
};

class NimbleMouse : public NimBLEServerCallbacks {
public:
  // name: 蓝牙设备名；manufacturer: 厂商名；battery: 初始电量(0-100)
  void begin(const char* name, const char* manufacturer, uint8_t battery) {
    NimBLEDevice::init(name);
    NimBLEDevice::setSecurityAuth(true, false, true);  // 绑定(bonding), 无 MITM, Secure Connections
    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(this);

    hid_   = new NimBLEHIDDevice(server_);
    input_ = hid_->inputReport(1);  // Report ID 1
    hid_->manufacturer()->setValue(manufacturer);
    hid_->pnp(0x02, 0xe502, 0xa111, 0x0210);
    hid_->hidInfo(0x00, 0x01);
    hid_->reportMap((uint8_t*)MJ_MOUSE_REPORT_MAP, sizeof(MJ_MOUSE_REPORT_MAP));
    hid_->setBatteryLevel(battery);
    hid_->startServices();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(HID_MOUSE);
    adv->addServiceUUID(hid_->hidService()->getUUID());
    adv->setScanResponse(true);
    adv->start();
  }

  bool isConnected() { return connected_; }

  // 相对移动 (x, y 为像素增量，范围 -127..127)
  void move(int8_t x, int8_t y, int8_t wheel = 0) {
    if (!connected_) return;
    uint8_t r[4] = {0x00, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
    input_->setValue(r, sizeof(r));
    input_->notify();
  }

  // 已配对(绑定)设备数量
  static int bondCount() { return NimBLEDevice::getNumBonds(); }

  // 清除全部蓝牙配对
  static void clearBonds() { NimBLEDevice::deleteAllBonds(); }

  // NimBLEServerCallbacks 回调
  void onConnect(NimBLEServer*) override { connected_ = true; }
  void onDisconnect(NimBLEServer*) override {
    connected_ = false;
    NimBLEDevice::startAdvertising();  // 断开后重新开始广播，便于重连
  }

private:
  NimBLEServer*         server_    = nullptr;
  NimBLEHIDDevice*      hid_       = nullptr;
  NimBLECharacteristic* input_     = nullptr;
  volatile bool         connected_ = false;
};
