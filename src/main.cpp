#include <Arduino.h>
#include <ArduinoHA.h>
#include <Adafruit_NeoPixel.h>
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

// Onboard WS2812 RGB LED of the Waveshare ESP32-S3-ETH
// is connected to GPIO21. It must be kept completely separate
// from the CC1101, which now uses GPIO1 as CS.
Adafruit_NeoPixel statusLed(1, 21, NEO_GRB + NEO_KHZ800);

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

HACover coverBuero("buero_rollo", HACover::PositionFeature);
HACover coverSchlafzimmer2("schlafzimmer2", HACover::PositionFeature);
HACover coverSchlafzimmer("schlafzimmer", HACover::PositionFeature);
HACover coverGaestezimmer2("gaestezimmer_rollo2", HACover::PositionFeature);
HACover coverKueche1("kueche_rollo1", HACover::PositionFeature);
HACover coverKueche2("kueche_rollo2", HACover::PositionFeature);
HACover coverBad("bad_rollo", HACover::PositionFeature);
HACover coverSchlafzimmerRollo1("schlafzimmer_rollo1", HACover::PositionFeature);
HACover coverWz4("wz4", HACover::PositionFeature);
HACover coverWohnzimmerTuer("wohnzimmer_rollo_tuer", HACover::PositionFeature);
HACover coverWohnzimmer2("wohnzimmer_rollo2", HACover::PositionFeature);
HACover coverWohnzimmer3("wohnzimmer_rollo3", HACover::PositionFeature);
HACover coverSchlafzimmer1("schlafzimmer1", HACover::PositionFeature);
HACover coverWohnzimmer("wohnzimmer", HACover::PositionFeature);

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

struct MovementState {
  bool active = false;
  bool opening = false;
  uint32_t startMs = 0;
  uint32_t durationMs = 0;
  int startPosition = 0;
  int targetPosition = 0;
  bool stopAtTarget = false;
};

MovementState movements[COVER_COUNT];

const char* const POSITION_MQTT_BASE = "aha/a55cdbdf7b37";

const char* const POSITION_COVER_IDS[COVER_COUNT] = {
  "buero_rollo", "schlafzimmer2", "schlafzimmer", "gaestezimmer_rollo2",
  "kueche_rollo1", "kueche_rollo2", "bad_rollo", "schlafzimmer_rollo1",
  "wz4", "wohnzimmer_rollo_tuer", "wohnzimmer_rollo2", "wohnzimmer_rollo3",
  "schlafzimmer1", "wohnzimmer"
};

const char* const POSITION_COVER_NAMES[COVER_COUNT] = {
  "Büro Rollo", "Schlafzimmer2", "Schlafzimmer", "Gästezimmer Rollo2",
  "Küche Rollo1", "Küche Rollo2", "Bad Rollo", "Schlafzimmer Rollo1",
  "wz4", "Wohnzimmer Rollo Tür", "Wohnzimmer Rollo2", "Wohnzimmer Rollo3",
  "Schlafzimmer1", "Wohnzimmer"
};

// Authoritative local position. ArduinoHA may report DefaultPosition for
// commands even though Home Assistant knows the actual position.
int knownPositions[COVER_COUNT] = {
  100,100,100,100,100,100,100,100,100,100,100,100,100,100
};

int clampPosition(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

void publishPositionState(size_t index) {
  if (index >= COVER_COUNT || !mqtt.isConnected()) return;

  char topic[160];
  snprintf(topic, sizeof(topic), "%s/%s/pos_t",
           POSITION_MQTT_BASE, POSITION_COVER_IDS[index]);

  char value[8];
  snprintf(value, sizeof(value), "%d", clampPosition(knownPositions[index]));
  mqtt.publish(topic, value, true);
}

void publishCoverState(size_t index, const char* state) {
  if (index >= COVER_COUNT || !mqtt.isConnected()) return;

  char topic[160];
  snprintf(topic, sizeof(topic), "%s/%s/stat_t",
           POSITION_MQTT_BASE, POSITION_COVER_IDS[index]);

  mqtt.publish(topic, state, true);
}

void publishCoverStateFromMovement(size_t index) {
  if (!movements[index].active) {
    const int pos = clampPosition(knownPositions[index]);
    publishCoverState(index,
      pos == 100 ? "open" :
      pos == 0 ? "closed" : "stopped");
  } else {
    publishCoverState(index,
      movements[index].opening ? "opening" : "closing");
  }
}

void updateMovement(size_t index) {
  MovementState& m = movements[index];
  if (!m.active) return;

  const uint32_t elapsed = millis() - m.startMs;

  if (elapsed >= m.durationMs) {
    m.active = false;
    knownPositions[index] = clampPosition(m.targetPosition);

    if (m.stopAtTarget && m.targetPosition != 0 && m.targetPosition != 100) {
      const auto id = coverBindings[index].id;
      if (!shutterControl.stop(id)) {
        Serial.println("CC1101: POSITION STOP SENDUNG FEHLGESCHLAGEN");
      }
    }

    publishPositionState(index);
    publishCoverStateFromMovement(index);
    return;
  }

  const float progress = (float)elapsed / (float)m.durationMs;
  const float position =
    m.startPosition +
    (m.targetPosition - m.startPosition) * progress;

  knownPositions[index] =
    clampPosition((int)(position + 0.5f));

  // MQTT pos_t is CURRENT physical position only.
  // The requested target is never written to pos_t.
  publishPositionState(index);
  publishCoverStateFromMovement(index);
}

void updateAllMovements() {
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();

  for (size_t i = 0; i < COVER_COUNT; i++) {
    updateMovement(i);
  }
}

void startMovement(size_t index, bool opening, int targetPosition = -1,
                     bool stopAtTarget = false,
                     bool commandAlreadySent = false) {
  MovementState& m = movements[index];
  const auto id = coverBindings[index].id;

  updateMovement(index);

  const int startPosition = clampPosition(knownPositions[index]);
  const int target = (targetPosition >= 0)
    ? clampPosition(targetPosition)
    : (opening ? 100 : 0);

  // If a new target arrives while already moving in the same direction,
  // retarget the current movement without transmitting RF a second time.
  if (m.active && m.opening == opening) {
    m.startPosition = startPosition;
    m.targetPosition = target;
    m.startMs = millis();
    m.stopAtTarget = stopAtTarget;

    const uint32_t fullDuration = opening
      ? shutterControl.openDurationMsFor(id)
      : shutterControl.closeDurationMsFor(id);
    const uint32_t distance = (uint32_t)abs(target - startPosition);
    m.durationMs = distance == 0 ? 0 : (fullDuration * distance) / 100UL;
    if (m.durationMs < 100 && distance != 0) m.durationMs = 100;

    publishPositionState(index);
    publishCoverStateFromMovement(index);
    return;
  }

  // Direction change while moving: stop first.
  if (m.active) {
    if (!shutterControl.stop(id)) {
      m.active = false;
      Serial.println("CC1101: POSITION STOP SENDUNG FEHLGESCHLAGEN");
      publishCoverState(index, "stopped");
      return;
    }
    m.active = false;
    knownPositions[index] = startPosition;
  }

  const uint32_t fullDuration = opening
    ? shutterControl.openDurationMsFor(id)
    : shutterControl.closeDurationMsFor(id);

  const uint32_t distance = (uint32_t)abs(target - startPosition);
  if (distance == 0) {
    knownPositions[index] = target;
    publishPositionState(index);
    publishCoverState(index,
      target == 100 ? "open" :
      target == 0 ? "closed" : "stopped");
    return;
  }

  m.active = true;
  m.opening = opening;
  m.startMs = millis();
  m.startPosition = startPosition;
  m.targetPosition = target;
  m.stopAtTarget = stopAtTarget;
  m.durationMs = (fullDuration * distance) / 100UL;
  if (m.durationMs < 100) m.durationMs = 100;

  if (targetPosition >= 0) {
    Serial.print("MQTT -> POSITION -> ");
    Serial.print(shutterControl.name(id));
    Serial.print(": ");
    Serial.print(startPosition);
    Serial.print("% -> ");
    Serial.print(target);
    Serial.print("% | ");
    Serial.print(m.durationMs);
    Serial.println(" ms");
  }

  if (!commandAlreadySent) {
    if (!(opening ? shutterControl.open(id) : shutterControl.close(id))) {
      m.active = false;
      Serial.println("CC1101: POSITION SENDUNG FEHLGESCHLAGEN");
      publishCoverState(index, "stopped");
      return;
    }
  }

  publishPositionState(index);
  publishCoverStateFromMovement(index);
}

// ============================================================
// MQTT target-position support for HomeKit/Home Assistant
// ============================================================
unsigned long positionDiscoveryDue = 0;

int findPositionCover(const char* topic) {
  if (!topic) return -1;
  for (size_t i = 0; i < COVER_COUNT; i++) {
    char expected[128];
    snprintf(expected, sizeof(expected), "%s/%s/set_pos_t",
             POSITION_MQTT_BASE, POSITION_COVER_IDS[i]);
    if (strcmp(topic, expected) == 0) return (int)i;
  }
  return -1;
}

void publishPositionDiscovery(size_t index) {
  if (index >= COVER_COUNT || !mqtt.isConnected()) return;

  char topic[160];
  snprintf(topic, sizeof(topic),
           "homeassistant/cover/a55cdbdf7b37/%s/config",
           POSITION_COVER_IDS[index]);

  char payload[1300];
  snprintf(payload, sizeof(payload),
    "{"
      "\"~\":\"%s/%s\", "
      "\"name\":\"%s\", "
      "\"uniq_id\":\"%s\", "
      "\"cmd_t\":\"~/cmd_t\", "
      "\"stat_t\":\"~/stat_t\", "
      "\"pos_t\":\"~/pos_t\", "
      "\"set_pos_t\":\"~/set_pos_t\", "
      "\"dev_cla\":\"shutter\", "
      "\"pl_open\":\"OPEN\", "
      "\"pl_clsd\":\"CLOSE\", "
      "\"pl_stop\":\"STOP\", "
      "\"stat_open\":\"open\", "
      "\"stat_opening\":\"opening\", "
      "\"stat_clsd\":\"closed\", "
      "\"stat_closing\":\"closing\", "
      "\"stat_stopped\":\"stopped\", "
      "\"pos_open\":100,"
      "\"pos_clsd\":0,"
      "\"optimistic\":false"
    "}",
    POSITION_MQTT_BASE,
    POSITION_COVER_IDS[index],
    POSITION_COVER_NAMES[index],
    POSITION_COVER_IDS[index]
  );

  mqtt.publish(topic, payload, true);
}

void publishAllPositionDiscovery() {
  for (size_t i = 0; i < COVER_COUNT; i++) {
    publishPositionDiscovery(i);
  }
}

void onPositionMqttMessage(const char* topic, const uint8_t* payload, uint16_t length) {
  if (!topic || !payload || length == 0) return;

  int index = -1;
  bool isCommand = false;
  bool isTarget = false;

  for (size_t i = 0; i < COVER_COUNT; i++) {
    char expectedTarget[128];
    char expectedCommand[128];

    snprintf(expectedTarget, sizeof(expectedTarget), "%s/%s/set_pos_t",
             POSITION_MQTT_BASE, POSITION_COVER_IDS[i]);
    snprintf(expectedCommand, sizeof(expectedCommand), "%s/%s/cmd_t",
             POSITION_MQTT_BASE, POSITION_COVER_IDS[i]);

    if (strcmp(topic, expectedTarget) == 0) {
      index = (int)i;
      isTarget = true;
      break;
    }
    if (strcmp(topic, expectedCommand) == 0) {
      index = (int)i;
      isCommand = true;
      break;
    }
  }

  if (index < 0) return;

  char value[16];
  const size_t n = min((size_t)length, sizeof(value) - 1);
  memcpy(value, payload, n);
  value[n] = '\0';

  if (isTarget) {
    const int target = atoi(value);
    if (target < 0 || target > 100) {
      Serial.print("Ungültige Zielposition: ");
      Serial.println(value);
      return;
    }

    const int current = clampPosition(knownPositions[index]);
    if (current == target) {
      publishPositionState(index);
      publishCoverState(index,
        target == 100 ? "open" :
        target == 0 ? "closed" : "stopped");
      return;
    }

    startMovement((size_t)index, target > current, target, true);
    return;
  }

  if (isCommand) {
    if (strcmp(value, "OPEN") == 0) {
      startMovement((size_t)index, true, 100, false);
    } else if (strcmp(value, "CLOSE") == 0) {
      startMovement((size_t)index, false, 0, false);
    } else if (strcmp(value, "STOP") == 0) {
      updateMovement((size_t)index);
      const auto id = coverBindings[index].id;
      if (shutterControl.stop(id)) {
        movements[index].active = false;
        publishPositionState(index);
        publishCoverState(index, "stopped");
      }
    }
  }
}


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

  mqtt.onMessage(onPositionMqttMessage);
  for (size_t i = 0; i < COVER_COUNT; i++) {
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/set_pos_t",
             POSITION_MQTT_BASE, POSITION_COVER_IDS[i]);
    mqtt.subscribe(topic);

    snprintf(topic, sizeof(topic), "%s/%s/cmd_t",
             POSITION_MQTT_BASE, POSITION_COVER_IDS[i]);
    mqtt.subscribe(topic);

    publishPositionState(i);
    publishCoverStateFromMovement(i);
  }

  positionDiscoveryDue = millis() + 1500;
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

      case HACover::CommandOpen: {
        Serial.print("HA -> OPEN -> ");
        Serial.println(shutterControl.name(id));

        ok = shutterControl.open(id);

        if (ok) {
          startMovement(i, true, -1, false, true);
        }
        break;
      }

      case HACover::CommandClose: {
        Serial.print("HA -> CLOSE -> ");
        Serial.println(shutterControl.name(id));

        ok = shutterControl.close(id);

        if (ok) {
          startMovement(i, false, -1, false, true);
        }
        break;
      }

      case HACover::CommandStop: {
        Serial.print("HA -> STOP -> ");
        Serial.println(shutterControl.name(id));

        updateMovement(i);
        const int stopPosition = clampPosition(knownPositions[i]);

        ok = shutterControl.stop(id);

        if (ok) {
          movements[i].active = false;
          sender->setPosition(stopPosition, true);
          sender->setState(HACover::StateStopped, true);
        }
        break;
      }

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

  // Keep the requested target position in the UI while the motor moves.
  // The firmware estimates the physical position internally, but must not
  // feed that intermediate value back into the Home Assistant/HomeKit slider.
  cover.setOptimistic(false);

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
  // Onboard RGB LED OFF
  // ----------------------------------------------------------

  statusLed.begin();
  statusLed.clear();
  statusLed.show();
  Serial.println("Onboard RGB LED: AUS");

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

  // Covers are exposed exclusively through MQTT discovery below.

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

  for (size_t i = 0; i < COVER_COUNT; i++) {
    movements[i] = MovementState{};
    knownPositions[i] = 100;
  }

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
  updateAllMovements();

  if (positionDiscoveryDue != 0 && (long)(millis() - positionDiscoveryDue) >= 0) {
    positionDiscoveryDue = 0;
    publishAllPositionDiscovery();
  }

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
