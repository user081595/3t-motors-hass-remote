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
// HomeKit / MQTT Positionssteuerung
// ============================================================
// Home Assistant sends a target position to set_pos_t.
// The discovery template adds the current HA position as:
//     TARGET|CURRENT
// Example: 50|100
//
// The ESP then sends OPEN/CLOSE via the existing CC1101 code and
// stops after the calculated travel time.

const char* const MQTT_BASE = "aha/a55cdbdf7b37";

struct ShutterMovement {
  bool active;
  bool opening;
  int currentPosition;
  int targetPosition;
  unsigned long startedAt;
  uint32_t durationMs;
};

ShutterMovement movements[COVER_COUNT];

const char* const COVER_IDS[COVER_COUNT] = {
  "buero_rollo",
  "schlafzimmer2",
  "schlafzimmer",
  "gaestezimmer_rollo2",
  "kueche_rollo1",
  "kueche_rollo2",
  "bad_rollo",
  "schlafzimmer_rollo1",
  "wz4",
  "wohnzimmer_rollo_tuer",
  "wohnzimmer_rollo2",
  "wohnzimmer_rollo3",
  "schlafzimmer1",
  "wohnzimmer"
};

const char* const COVER_NAMES[COVER_COUNT] = {
  "Büro Rollo",
  "Schlafzimmer2",
  "Schlafzimmer",
  "Gästezimmer Rollo2",
  "Küche Rollo1",
  "Küche Rollo2",
  "Bad Rollo",
  "Schlafzimmer Rollo1",
  "wz4",
  "Wohnzimmer Rollo Tür",
  "Wohnzimmer Rollo2",
  "Wohnzimmer Rollo3",
  "Schlafzimmer1",
  "Wohnzimmer"
};

int findCoverByTopic(const char* topic) {
  if (!topic) return -1;

  for (size_t i = 0; i < COVER_COUNT; i++) {
    char expected[128];
    snprintf(expected, sizeof(expected), "%s/%s/set_pos_t",
             MQTT_BASE, COVER_IDS[i]);

    if (strcmp(topic, expected) == 0) {
      return (int)i;
    }
  }

  return -1;
}

void publishPosition(size_t index, int position) {
  if (index >= COVER_COUNT) return;

  position = constrain(position, 0, 100);

  char topic[128];
  snprintf(topic, sizeof(topic), "%s/%s/pos_t",
           MQTT_BASE, COVER_IDS[index]);

  char payload[8];
  snprintf(payload, sizeof(payload), "%d", position);

  mqtt.publish(topic, payload);
}

void finishMovement(size_t index) {
  if (index >= COVER_COUNT) return;

  ShutterMovement &m = movements[index];

  if (!m.active) return;

  const auto id = coverBindings[index].id;

  // Send the real STOP command at the calculated target.
  shutterControl.stop(id);

  m.currentPosition = m.targetPosition;
  m.active = false;

  publishPosition(index, m.currentPosition);

  if (m.currentPosition <= 0) {
    coverBindings[index].cover->setState(HACover::StateClosed);
  } else if (m.currentPosition >= 100) {
    coverBindings[index].cover->setState(HACover::StateOpen);
  } else {
    coverBindings[index].cover->setState(HACover::StateStopped);
  }

  Serial.print("POSITION erreicht: ");
  Serial.print(COVER_NAMES[index]);
  Serial.print(" -> ");
  Serial.print(m.currentPosition);
  Serial.println("%");
}

void startPositionMovement(size_t index, int currentPosition, int targetPosition) {
  if (index >= COVER_COUNT) return;

  currentPosition = constrain(currentPosition, 0, 100);
  targetPosition = constrain(targetPosition, 0, 100);

  ShutterMovement &m = movements[index];
  const auto id = coverBindings[index].id;

  // Stop an already running movement before starting a new one.
  if (m.active) {
    shutterControl.stop(id);
    m.active = false;
  }

  m.currentPosition = currentPosition;
  m.targetPosition = targetPosition;

  if (currentPosition == targetPosition) {
    publishPosition(index, targetPosition);
    coverBindings[index].cover->setCurrentPosition(targetPosition);
    coverBindings[index].cover->setState(
      targetPosition == 0 ? HACover::StateClosed :
      targetPosition == 100 ? HACover::StateOpen :
      HACover::StateStopped
    );
    Serial.print("POSITION bereits erreicht: ");
    Serial.print(COVER_NAMES[index]);
    Serial.print(" -> ");
    Serial.print(targetPosition);
    Serial.println("%");
    return;
  }

  m.opening = targetPosition > currentPosition;

  uint32_t fullDuration = m.opening
    ? shutterControl.openDurationMsFor(id)
    : shutterControl.closeDurationMsFor(id);

  uint32_t distance = (uint32_t)abs(targetPosition - currentPosition);

  m.durationMs = (fullDuration * distance) / 100UL;

  if (m.durationMs < 100) m.durationMs = 100;

  Serial.print("HA -> POSITION -> ");
  Serial.print(COVER_NAMES[index]);
  Serial.print(": ");
  Serial.print(currentPosition);
  Serial.print("% -> ");
  Serial.print(targetPosition);
  Serial.print("% | ");
  Serial.print(m.durationMs);
  Serial.println(" ms");

  bool ok = m.opening
    ? shutterControl.open(id)
    : shutterControl.close(id);

  if (!ok) {
    Serial.println("CC1101: POSITION SENDUNG FEHLGESCHLAGEN");
    m.active = false;
    return;
  }

  m.startedAt = millis();
  m.active = true;

  coverBindings[index].cover->setState(
    m.opening ? HACover::StateOpening : HACover::StateClosing
  );

  publishPosition(index, currentPosition);
}

void updateMovements() {
  unsigned long now = millis();

  for (size_t i = 0; i < COVER_COUNT; i++) {
    ShutterMovement &m = movements[i];

    if (!m.active) continue;

    unsigned long elapsed = now - m.startedAt;

    if (elapsed >= m.durationMs) {
      finishMovement(i);
      continue;
    }

    int start = m.currentPosition;
    int target = m.targetPosition;

    int delta = target - start;
    int position = start + (int)((int64_t)delta * elapsed / m.durationMs);

    if (position != start) {
      coverBindings[i].cover->setPosition(position);
    }
  }
}

void publishPositionDiscovery(size_t index) {
  if (index >= COVER_COUNT) return;

  char configTopic[160];
  char baseTopic[128];

  snprintf(baseTopic, sizeof(baseTopic),
           "%s/%s", MQTT_BASE, COVER_IDS[index]);

  // Same discovery topic/object/unique_id as the existing ArduinoHA
  // cover, but with the missing set_position_topic added.
  snprintf(configTopic, sizeof(configTopic),
           "homeassistant/cover/a55cdbdf7b37/%s/config",
           COVER_IDS[index]);

  char payload[1100];

  snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"~\":\"%s\","
      "\"name\":\"%s\","
      "\"uniq_id\":\"%s\","
      "\"cmd_t\":\"~/cmd_t\","
      "\"stat_t\":\"~/stat_t\","
      "\"pos_t\":\"~/pos_t\","
      "\"set_pos_t\":\"~/set_pos_t\","
      "\"set_pos_tpl\":\"{{ position }}|{{ state_attr(entity_id, 'current_position') | int(0) }}\","
      "\"dev_cla\":\"shutter\","
      "\"pl_open\":\"OPEN\","
      "\"pl_clsd\":\"CLOSE\","
      "\"pl_stop\":\"STOP\","
      "\"stat_open\":\"open\","
      "\"stat_opening\":\"opening\","
      "\"stat_clsd\":\"closed\","
      "\"stat_closing\":\"closing\","
      "\"stat_stopped\":\"stopped\","
      "\"pos_open\":100,"
      "\"pos_clsd\":0,"
      "\"opt\":false"
    "}",
    baseTopic,
    COVER_NAMES[index],
    COVER_IDS[index]
  );

  mqtt.publish(configTopic, payload, true);

  Serial.print("HK Position Discovery: ");
  Serial.println(COVER_IDS[index]);
}

void publishAllPositionDiscovery() {
  for (size_t i = 0; i < COVER_COUNT; i++) {
    publishPositionDiscovery(i);
  }
}

void onMqttMessage(const char* topic, const uint8_t* payload, uint16_t length) {
  if (!topic || !payload || length == 0) return;

  int index = findCoverByTopic(topic);
  if (index < 0) return;

  char buffer[32];
  size_t copyLength = length;
  if (copyLength >= sizeof(buffer)) copyLength = sizeof(buffer) - 1;

  memcpy(buffer, payload, copyLength);
  buffer[copyLength] = '\0';

  int target = -1;
  int current = -1;

  char* separator = strchr(buffer, '|');

  if (separator) {
    *separator = '\0';
    target = atoi(buffer);
    current = atoi(separator + 1);
  } else {
    target = atoi(buffer);
  }

  if (target < 0 || target > 100) {
    Serial.print("Ungültige Zielposition: ");
    Serial.println(buffer);
    return;
  }

  // If HA didn't provide the current position, use our last known value.
  if (current < 0 || current > 100) {
    current = movements[index].currentPosition;

    if (current < 0 || current > 100) {
      Serial.println("Keine aktuelle Position bekannt.");
      Serial.println("Bitte einmal in HA die aktuelle Position setzen.");
      return;
    }
  }

  startPositionMovement(index, current, target);
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

bool positionDiscoveryPending = false;
unsigned long positionDiscoveryAt = 0;

void onMqttConnected() {
  Serial.println();
  Serial.println("********************************");
  Serial.println("MQTT: VERBUNDEN");
  Serial.println("********************************");

  statusSensor.setValue("online");

  mqtt.onMessage(onMqttMessage);

  for (size_t i = 0; i < COVER_COUNT; i++) {
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/set_pos_t",
             MQTT_BASE, COVER_IDS[i]);
    mqtt.subscribe(topic);
  }

  // ArduinoHA publishes its own discovery during connection.
  // We overwrite it shortly afterwards with the identical cover
  // plus set_position_topic.
  positionDiscoveryPending = true;
  positionDiscoveryAt = millis() + 1500;
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

  for (size_t i = 0; i < COVER_COUNT; i++) {
    movements[i].active = false;
    movements[i].opening = false;
    movements[i].currentPosition = -1;
    movements[i].targetPosition = -1;
    movements[i].startedAt = 0;
    movements[i].durationMs = 0;
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
  updateMovements();

  if (positionDiscoveryPending &&
      (long)(millis() - positionDiscoveryAt) >= 0 &&
      mqtt.isConnected()) {
    positionDiscoveryPending = false;
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
