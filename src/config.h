#pragma once
#include <Arduino.h>
#include <vector>

struct WifiConfig {
  String ssid = "";
  String password = "";
};

struct MqttConfig {
  String host = "";
  uint16_t port = 1883;
  String username = "";
  String password = "";
};

struct Shutter {
  String id;
  String name;
  char signal[66];
};

struct Config {
  String deviceId = "";
  WifiConfig wifiConfig;
  MqttConfig mqttConfig;
  std::vector<Shutter> shutters;
};
