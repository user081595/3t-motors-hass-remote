#pragma once

#include <Arduino.h>
#include <vector>

struct WifiConfig {
  String ssid = "";
  String password = "";
};

struct MqttConfig {
  String host = "192.168.188.104";
  uint16_t port = 1883;
  String username = "Test";
  String password = "Test";
};

struct Shutter {
  String id;
  String name;
  char signal[66] = {};
};

struct Config {
  String deviceId = "ESP32-S3-Rollladen";
  
  WifiConfig wifiConfig;
  MqttConfig mqttConfig;

  std::vector<Shutter> shutters;
};
