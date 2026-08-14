#include <Arduino.h>
#include <ArduinoHA.h>
#include <ETH.h>
#include <Network.h>
#include <config.h>
#include <ShutterControl.h>

Config config;

// ============================================================
// W5500
// ============================================================

#define ETH_PHY_TYPE   ETH_PHY_W5500
#define ETH_PHY_ADDR   1

#define ETH_PHY_CS     14
#define ETH_PHY_IRQ    10
#define ETH_PHY_RST     9

#define ETH_SPI_SCK    13
#define ETH_SPI_MISO   12
#define ETH_SPI_MOSI   11

IPAddress local_ip(192, 168, 188, 250);
IPAddress gateway(192, 168, 188, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 188, 1);

bool ethConnected = false;

// ============================================================
// Home Assistant / MQTT
// ============================================================

NetworkClient client;

byte mac[] = {
  0xA5, 0x5C, 0xDB, 0xDF, 0x7B, 0x37
};

HADevice device(mac, sizeof(mac));
HAMqtt mqtt(client, device);

HASensor statusSensor("status");

ShutterControl shutterControl;

// ============================================================
// HA Covers
// ============================================================

HACover coverBuero("buero_rollo");
HACover coverSchlafzimmer2("schlafzimmer2");
HACover coverSchlafzimmer("schlafzimmer");
HACover coverGaestezimmer2("gaestezimmer_rollo2");
HACover coverKueche1("kueche_rollo1");
HACover coverKueche2("kueche_rollo2");
HACover coverBad("bad_rollo");
HACover coverSchlafzimmerRollo1("schlafzimmer_rollo1");
HACover coverWz4("wz4");
HACover coverWohnzimmerTuer("wohnzimmer_rollo_tuer");
HACover coverWohnzimmer2("wohnzimmer_rollo2");
HACover coverWohnzimmer3("wohnzimmer_rollo3");
HACover coverSchlafzimmer1("schlafzimmer1");
HACover coverWohnzimmer("wohnzimmer");

struct CoverBinding {
  HACover* cover;
  ShutterControl::ShutterId id;
};

CoverBinding coverBindings[] = {
  { &coverBuero,             ShutterControl::Buero_Rollo },
  { &coverSchlafzimmer2,     ShutterControl::Schlafzimmer2 },
  { &coverSchlafzimmer,      ShutterControl::Schlafzimmer },
  { &coverGaestezimmer2,     ShutterControl::Gaestezimmer_Rollo2 },
  { &coverKueche1,           ShutterControl::Kueche_Rollo1 },
  { &coverKueche2,           ShutterControl::Kueche_Rollo2 },
  { &coverBad,               ShutterControl::Bad_Rollo },
  { &coverSchlafzimmerRollo1,ShutterControl::Schlafzimmer_Rollo1 },
  { &coverWz4,               ShutterControl::wz4 },
  { &coverWohnzimmerTuer,    ShutterControl::Wohnzimmer_Rollo_Tuer },
  { &coverWohnzimmer2,       ShutterControl::Wohnzimmer_Rollo2 },
  { &coverWohnzimmer3,       ShutterControl::Wohnzimmer_Rollo3 },
  { &coverSchlafzimmer1,     ShutterControl::Schlafzimmer1 },
  { &coverWohnzimmer,        ShutterControl::Wohnzimmer }
};

const size_t COVER_COUNT =
  sizeof(coverBindings) / sizeof(coverBindings[0]);

// ============================================================
// Ethernet events
// ============================================================

void onNetworkEvent(
  arduino_event_id_t event,
  arduino_event_info_t info
) {
  switch (event) {

    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH gestartet");
      ETH.setHostname("esp32-s3-shutter");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH verbunden");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("ETH IP erhalten");
      Serial.print("IP: ");      Serial.println(ETH.localIP());
      Serial.print("Gateway: "); Serial.println(ETH.gatewayIP());
      Serial.print("Subnet: ");  Serial.println(ETH.subnetMask());
      ethConnected = true;
      break;

    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("ETH IP verloren");
      ethConnected = false;
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH getrennt");
      ethConnected = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH gestoppt");
      ethConnected = false;
      break;

    default:
      break;
  }
}

// ============================================================
// MQTT events
// ============================================================

void onMqttConnected() {
  Serial.println();
  Serial.println("********************************");
  Serial.println("MQTT: VERBUNDEN");
  Serial.println("********************************");

  statusSensor.setValue("online");
}

void onMqttDisconnected() {
  Serial.println();
  Serial.println("********************************");
  Serial.println("MQTT: GETRENNT");
  Serial.println("********************************");

  statusSensor.setValue("offline");
}

void onMqttStateChanged(HAMqtt::ConnectionState state) {
  Serial.print("MQTT State: ");

  switch (state) {
    case HAMqtt::StateConnecting:
      Serial.println("CONNECTING");
      break;

    case HAMqtt::StateConnected:
      Serial.println("CONNECTED");
      break;

    case HAMqtt::StateConnectionTimeout:
      Serial.println("CONNECTION TIMEOUT");
      break;

    case HAMqtt::StateConnectionLost:
      Serial.println("CONNECTION LOST");
      break;

    case HAMqtt::StateConnectionFailed:
      Serial.println("CONNECTION FAILED");
      break;

    case HAMqtt::StateDisconnected:
      Serial.println("DISCONNECTED");
      break;

    case HAMqtt::StateBadProtocol:
      Serial.println("BAD PROTOCOL");
      break;

    case HAMqtt::StateBadClientId:
      Serial.println("BAD CLIENT ID");
      break;

    case HAMqtt::StateUnavailable:
      Serial.println("UNAVAILABLE");
      break;

    case HAMqtt::StateBadCredentials:
      Serial.println("BAD CREDENTIALS");
      break;

    case HAMqtt::StateUnauthorized:
      Serial.println("UNAUTHORIZED");
      break;

    default:
      Serial.println("UNKNOWN");
      break;
  }
}

// ============================================================
// Cover command callback
// ============================================================

void onCoverCommand(
  HACover::CoverCommand command,
  HACover* sender
) {
  for (size_t i = 0; i < COVER_COUNT; i++) {

    if (coverBindings[i].cover != sender) {
      continue;
    }

    const auto id = coverBindings[i].id;

    bool ok = false;

    switch (command) {

      case HACover::CommandOpen:
        Serial.print("HA -> OPEN -> ");
        Serial.println(shutterControl.name(id));

        ok = shutterControl.open(id);

        if (ok) {
          sender->setState(HACover::StateOpen);
        }
        break;

      case HACover::CommandClose:
        Serial.print("HA -> CLOSE -> ");
        Serial.println(shutterControl.name(id));

        ok = shutterControl.close(id);

        if (ok) {
          sender->setState(HACover::StateClosed);
        }
        break;

      case HACover::CommandStop:
        Serial.print("HA -> STOP -> ");
        Serial.println(shutterControl.name(id));

        ok = shutterControl.stop(id);

        if (ok) {
          sender->setState(HACover::StateStopped);
        }
        break;

      default:
        break;
    }

    if (!ok) {
      Serial.println("CC1101: SENDUNG FEHLGESCHLAGEN");
    }

    return;
  }
}

// ============================================================
// Cover setup
// ============================================================

void setupCover(
  HACover& cover,
  const char* icon = "mdi:window-shutter"
) {
  cover.setDeviceClass("shutter");
  cover.setIcon(icon);

  // There is no position feedback from the 433 MHz motors.
  // Therefore HA should treat commands optimistically.
  cover.setOptimistic(true);

  cover.onCommand(onCoverCommand);
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-S3 ROLLLADEN CONTROLLER");
  Serial.println("Ethernet + MQTT + CC1101");
  Serial.println("Broadlink RF -> CC1101");
  Serial.println("================================");

  // ----------------------------------------------------------
  // Ethernet
  // ----------------------------------------------------------

  Network.onEvent(onNetworkEvent);

  Serial.println();
  Serial.println("Starte W5500...");

  bool ethResult = ETH.begin(
    ETH_PHY_TYPE,
    ETH_PHY_ADDR,
    ETH_PHY_CS,
    ETH_PHY_IRQ,
    ETH_PHY_RST,
    SPI3_HOST,
    ETH_SPI_SCK,
    ETH_SPI_MISO,
    ETH_SPI_MOSI
  );

  Serial.print("ETH.begin(): ");
  Serial.println(ethResult ? "OK" : "FEHLER");

  if (ETH.config(
        local_ip,
        gateway,
        subnet,
        dns
      )) {
    Serial.println("Statische IP: OK");
  } else {
    Serial.println("Statische IP: FEHLER");
  }

  Serial.println("Warte auf Ethernet...");

  unsigned long start = millis();

  while (!ethConnected && millis() - start < 15000) {
    delay(100);
  }

  if (!ethConnected) {
    Serial.println("!!! ETHERNET NICHT VERBUNDEN !!!");
    return;
  }

  Serial.println("Ethernet ist bereit.");

  // ----------------------------------------------------------
  // HA device
  // ----------------------------------------------------------

  device.setName("ESP32-S3 Rollladen Controller");
  device.setSoftwareVersion("1.0.0");

  statusSensor.setName("Status");
  statusSensor.setIcon("mdi:lan-connect");

  setupCover(coverBuero);
  setupCover(coverSchlafzimmer2);
  setupCover(coverSchlafzimmer);
  setupCover(coverGaestezimmer2);
  setupCover(coverKueche1);
  setupCover(coverKueche2);
  setupCover(coverBad);
  setupCover(coverSchlafzimmerRollo1);
  setupCover(coverWz4);
  setupCover(coverWohnzimmerTuer);
  setupCover(coverWohnzimmer2);
  setupCover(coverWohnzimmer3);
  setupCover(coverSchlafzimmer1);
  setupCover(coverWohnzimmer);

  // ----------------------------------------------------------
  // MQTT
  // ----------------------------------------------------------

  mqtt.onConnected(onMqttConnected);
  mqtt.onDisconnected(onMqttDisconnected);
  mqtt.onStateChanged(onMqttStateChanged);

  Serial.println();
  Serial.println("Starte MQTT...");

  Serial.print("Broker: ");
  Serial.println(config.mqttConfig.host);

  Serial.print("Port: ");
  Serial.println(config.mqttConfig.port);

  Serial.print("Benutzer: ");
  Serial.println(config.mqttConfig.username);

  mqtt.begin(
    config.mqttConfig.host.c_str(),
    config.mqttConfig.port,
    config.mqttConfig.username.c_str(),
    config.mqttConfig.password.c_str()
  );

  Serial.println("MQTT gestartet");

  // ----------------------------------------------------------
  // CC1101
  // ----------------------------------------------------------

  shutterControl.setup();

  Serial.println();
  Serial.println("================================");
  Serial.println("SETUP FERTIG");
  Serial.println("================================");
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  mqtt.loop();

  static unsigned long lastMqttCheck = 0;

  if (millis() - lastMqttCheck >= 2000) {

    lastMqttCheck = millis();

    Serial.print("MQTT Status: ");

    if (mqtt.isConnected()) {
      Serial.println("VERBUNDEN");
      statusSensor.setValue("online");
    } else {
      Serial.println("NICHT VERBUNDEN");
    }
  }

  delay(10);
}
