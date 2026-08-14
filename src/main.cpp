#include <Arduino.h>
#include <ArduinoHA.h>
#include <config.h>
#include <ShutterControl.cpp>
#include <ShutterRead.h>
#include <vector>
#include <WiFi.h>

WiFiClient client;

Config config;


byte mac[] = {0xA5, 0x5C, 0xDB, 0xDF, 0x7B, 0x37}; // change your mac address
HADevice device(mac, sizeof(mac));
HAMqtt mqtt(client, device);
HACover recorder("shutter_recorder"); // Shutter to record existing devices. Up/Down should work after recording. Stop button will start/stop recording
HASensor lastScannedShutter("last_scanned_shutter"); // this is a sensor that will show the last recorded shutter id
ShutterControl shutterControl;

bool isRecording = false;

// home assistant gives us an id, use this id to find the shutter with the right signal
Shutter* findShutterById(const String& id) {
  for (Shutter& s : config.shutters) {
    if (s.id == id) {
      return &s;
    }
  }
  return nullptr;
}

void onCoverCommand(HACover::CoverCommand cmd, HACover* sender) {
    Shutter* s = findShutterById(sender->uniqueId());
    if (cmd == HACover::CommandOpen) {
        shutterControl.openShutter(s->signal);
        Serial.println("Command: Open");
        sender->setState(HACover::StateOpen);
    } else if (cmd == HACover::CommandClose) {
        shutterControl.closeShutter(s->signal);
        Serial.println("Command: Close");
        sender->setState(HACover::StateClosed);
    } else if (cmd == HACover::CommandStop) {
        shutterControl.stopShutter(s->signal);
        Serial.println("Command: Stop");
        sender->setState(HACover::StateStopped);
    }
}

// will be executed if a button is pressed for the recorder
void onCoverCommandRecorder(HACover::CoverCommand cmd, HACover* sender) {
    if (cmd == HACover::CommandOpen) {
        shutterControl.openShutter(recordedShutterId);
        Serial.println("Command: Open last recorded");
        sender->setState(HACover::StateOpen); // report state back to the HA
    } else if (cmd == HACover::CommandClose) {
        shutterControl.closeShutter(recordedShutterId);
        Serial.println("Command: Close last recorded");
        sender->setState(HACover::StateClosed); // report state back to the HA
    } else if (cmd == HACover::CommandStop) {
      if (!isRecording) {
        Serial.println("Start recording signal");
        lastScannedShutter.setValue("Recording");
        isRecording = true;
        startReceive();
      } else {
        Serial.println("Stopped recording signal");
        stopRecording();
        shutterControl.setup();
        if (extractShutterIdFromTimings()) {
          Serial.println("Found a shutter with the device id:");
          Serial.println(recordedShutterId);
          lastScannedShutter.setValue(recordedShutterId);
        } else {
          Serial.println("No shutter id found while recording");
        }
      }
    }
}

void setupShutters() {
    // create all shutters
    for(const Shutter& s : config.shutters) {
      auto* shutter = new HACover(s.id.c_str());
      shutter->setName(s.name.c_str());
      shutter->setDeviceClass("shutter");
      shutter->setIcon("mdi:window-shutter");
      shutter->setOptimistic(true);
      shutter->onCommand(onCoverCommand);
      shutter->setCurrentState(HACover::StateUnknown);
    }
    // shutter to record signal and test it
    recorder.setDeviceClass("damper");
    recorder.setName("Record Shutter Signal");
    recorder.onCommand(onCoverCommandRecorder);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(
    config.wifiConfig.ssid,
    config.wifiConfig.password
  );

  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }
  Serial.print("Successfully connected to WiFi");

  device.setName(config.deviceId.c_str());
  device.setSoftwareVersion("1.0.0");
  shutterControl.setup();
  setupShutters();

  // add sensor to that the last scanned shutter id will be sent to
  lastScannedShutter.setName("Last Scanned Shutter");
  lastScannedShutter.setIcon("mdi:broadcast");

  mqtt.begin(
    config.mqttConfig.host.c_str(),
    config.mqttConfig.port,
    config.mqttConfig.username.c_str(),
    config.mqttConfig.password.c_str()
  );

  mqtt.loop();
  lastScannedShutter.setValue("-");
}

void loop() {
  mqtt.loop();
}
