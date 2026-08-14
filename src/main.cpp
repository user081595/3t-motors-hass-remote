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

// ============================================================
// Custom MQTT position command support
//
// ArduinoHA HACover exposes OPEN/CLOSE/STOP through cmd_t, but
// its generated discovery does not expose set_position_topic.
// Home Assistant therefore needs a small discovery override and
// a custom set_pos_t subscription so HomeKit percentages can
// reach the ESP32.
//
// The screenshots supplied by the user confirm the ArduinoHA
// data-topic layout: aha/<device-id>/<cover-id>/cmd_t and
// aha/<device-id>/<cover-id>/pos_t.
// ============================================================

static const char* MQTT_DEVICE_ID = "a55cdbdf7b37";

static const char* COVER_TOPIC_IDS[COVER_COUNT] = {
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

static const char* COVER_NAMES[COVER_COUNT] = {
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

bool customDiscoveryPending = false;
uint32_t customDiscoveryAt = 0;



// Forward declaration.
void startPositionMovement(size_t index, int targetPosition);

size_t findCoverIndexByPositionTopic(const char* topic) {
  if (!topic) return COVER_COUNT;

  char expected[128];

  for (size_t i = 0; i < COVER_COUNT; i++) {
    snprintf(
      expected,
      sizeof(expected),
      "aha/%s/%s/set_pos_t",
      MQTT_DEVICE_ID,
      COVER_TOPIC_IDS[i]
    );

    if (strcmp(topic, expected) == 0) {
      return i;
    }
  }

  return COVER_COUNT;
}

// MQTT callback for the custom set_position_topic.
void onMqttMessage(const char* topic, const uint8_t* payload, uint16_t length) {

  const size_t index = findCoverIndexByPositionTopic(topic);
  if (index >= COVER_COUNT) {
    return;
  }

  char value[16];
  const size_t copyLength =
    (length < sizeof(value) - 1) ? length : sizeof(value) - 1;

  memcpy(value, payload, copyLength);
  value[copyLength] = '\0';

  char* endPtr = nullptr;
  long requested = strtol(value, &endPtr, 10);

  if (endPtr == value) {
    Serial.print("MQTT Position: ungueltiger Wert: ");
    Serial.println(value);
    return;
  }

  requested = constrain(requested, 0L, 100L);

  Serial.print("HA -> POSITION -> ");
  Serial.print(COVER_NAMES[index]);
  Serial.print(" -> ");
  Serial.print(requested);
  Serial.println("%");

  startPositionMovement(index, (int)requested);
}

// Publish a discovery override for one cover. The unique_id and
// entity_id stay identical to the ArduinoHA cover; we only add
// set_position_topic so Home Assistant/HomeKit can send a target.
void publishCoverDiscovery(size_t index) {

  char topic[128];
  snprintf(
    topic,
    sizeof(topic),
    "homeassistant/cover/%s/%s/config",
    MQTT_DEVICE_ID,
    COVER_TOPIC_IDS[index]
  );

  char payload[1024];

  snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"name\":\"%s\","
      "\"uniq_id\":\"%s\","
      "\"obj_id\":\"%s\","
      "\"dev_cla\":\"shutter\","
      "\"cmd_t\":\"aha/%s/%s/cmd_t\","
      "\"stat_t\":\"aha/%s/%s/stat_t\","
      "\"pos_t\":\"aha/%s/%s/pos_t\","
      "\"set_pos_t\":\"aha/%s/%s/set_pos_t\","
      "\"pl_open\":\"OPEN\","
      "\"pl_close\":\"CLOSE\","
      "\"pl_stop\":\"STOP\","
      "\"stat_open\":\"open\","
      "\"stat_clsd\":\"closed\","
      "\"stat_stopped\":\"stopped\","
      "\"pos_open\":100,"
      "\"pos_clsd\":0,"
      "\"opt\":false,"
      "\"dev\":{\"ids\":[\"%s\"],\"name\":\"ESP32-S3 Rollladen Controller\"}"
    "}",
    COVER_NAMES[index],
    COVER_TOPIC_IDS[index],
    COVER_TOPIC_IDS[index],
    MQTT_DEVICE_ID, COVER_TOPIC_IDS[index],
    MQTT_DEVICE_ID, COVER_TOPIC_IDS[index],
    MQTT_DEVICE_ID, COVER_TOPIC_IDS[index],
    MQTT_DEVICE_ID, COVER_TOPIC_IDS[index],
    MQTT_DEVICE_ID
  );

  if (mqtt.publish(topic, payload, true)) {
    Serial.print("Discovery Position aktiv: ");
    Serial.println(COVER_NAMES[index]);
  } else {
    Serial.print("Discovery Position FEHLER: ");
    Serial.println(COVER_NAMES[index]);
  }
}

void publishAllCoverDiscovery() {
  for (size_t i = 0; i < COVER_COUNT; i++) {
    publishCoverDiscovery(i);
    delay(20);
  }
}

struct MovementState {
  bool active = false;
  bool opening = false;
  uint32_t startMs = 0;
  uint32_t durationMs = 0;
  int startPosition = 0;
  int targetPosition = 0;
};

MovementState movements[COVER_COUNT];

int clampPosition(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

int getKnownOrAssumedPosition(HACover* cover, bool opening) {
  const int16_t pos = cover->getCurrentPosition();
  if (pos == HACover::DefaultPosition || pos < 0 || pos > 100) {
    return opening ? 0 : 100;
  }
  return clampPosition(pos);
}

void updateMovement(size_t index) {
  MovementState& m = movements[index];
  if (!m.active) return;

  HACover* cover = coverBindings[index].cover;
  const uint32_t elapsed = millis() - m.startMs;

  if (elapsed >= m.durationMs) {
    m.active = false;
    cover->setPosition(m.targetPosition, true);
    cover->setState(
      m.targetPosition == 100 ? HACover::StateOpen : HACover::StateClosed,
      true
    );
    return;
  }

  const float progress = (float)elapsed / (float)m.durationMs;
  const float position =
    m.startPosition +
    (m.targetPosition - m.startPosition) * progress;

  cover->setPosition(clampPosition((int)(position + 0.5f)));
}

void updateAllMovements() {
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < 200) return;
  lastUpdate = millis();

  for (size_t i = 0; i < COVER_COUNT; i++) {
    updateMovement(i);
  }
}

void startMovement(size_t index, bool opening) {
  MovementState& m = movements[index];
  HACover* cover = coverBindings[index].cover;
  const auto id = coverBindings[index].id;

  // If a previous movement is still running, first calculate its current position.
  updateMovement(index);

  const int startPosition = getKnownOrAssumedPosition(cover, opening);
  m.active = true;
  m.opening = opening;
  m.startMs = millis();
  m.startPosition = startPosition;
  m.targetPosition = opening ? 100 : 0;
  m.durationMs = opening
    ? shutterControl.openDurationMsFor(id)
    : shutterControl.closeDurationMsFor(id);

  // Publish the starting position immediately.
  // Do NOT publish StateOpening/StateClosing here: HomeKit can interpret
  // those transient MQTT states as the final cover state. The position
  // itself is updated continuously below and the final state is published
  // only when the measured travel time has elapsed.
  cover->setPosition(startPosition, true);
}


// Start a movement to an arbitrary target position (0-100).
// The RF protocol itself only has OPEN/CLOSE/STOP, so the target
// is reached by sending OPEN or CLOSE and stopping after the
// corresponding fraction of the measured travel time.
void startPositionMovement(size_t index, int targetPosition) {

  targetPosition = clampPosition(targetPosition);

  MovementState& m = movements[index];
  HACover* cover = coverBindings[index].cover;
  const auto id = coverBindings[index].id;

  // Finish the currently estimated position before replacing it.
  updateMovement(index);

  int startPosition = cover->getCurrentPosition();

  if (
    startPosition == HACover::DefaultPosition ||
    startPosition < 0 ||
    startPosition > 100
  ) {
    // If no position is known, only the two endpoints are safe
    // assumptions. For an intermediate target we assume the
    // opposite endpoint depending on the requested direction.
    startPosition = (targetPosition > 50) ? 0 : 100;
  }

  startPosition = clampPosition(startPosition);

  if (startPosition == targetPosition) {
    m.active = false;
    cover->setPosition(targetPosition, true);
    cover->setState(
      targetPosition == 100
        ? HACover::StateOpen
        : HACover::StateClosed,
      true
    );
    Serial.print("HA -> POSITION bereits erreicht: ");
    Serial.print(COVER_NAMES[index]);
    Serial.print(" -> ");
    Serial.print(targetPosition);
    Serial.println("%");
    return;
  }

  const bool opening = targetPosition > startPosition;

  // Fraction of the full travel required.
  const float fraction =
    opening
      ? (float)(targetPosition - startPosition) / 100.0f
      : (float)(startPosition - targetPosition) / 100.0f;

  const uint32_t fullDuration =
    opening
      ? shutterControl.openDurationMsFor(id)
      : shutterControl.closeDurationMsFor(id);

  const uint32_t duration =
    (uint32_t)((float)fullDuration * fraction + 0.5f);

  if (duration == 0) {
    m.active = false;
    cover->setPosition(targetPosition, true);
    return;
  }

  Serial.print("Position: ");
  Serial.print(startPosition);
  Serial.print("% -> ");
  Serial.print(targetPosition);
  Serial.print("% | ");
  Serial.print(duration);
  Serial.println(" ms");

  bool ok =
    opening
      ? shutterControl.open(id)
      : shutterControl.close(id);

  if (!ok) {
    Serial.println("CC1101: POSITION SENDUNG FEHLGESCHLAGEN");
    return;
  }

  m.active = true;
  m.opening = opening;
  m.startMs = millis();
  m.startPosition = startPosition;
  m.targetPosition = targetPosition;
  m.durationMs = duration;

  cover->setPosition(startPosition, true);
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

  // Re-apply the discovery override shortly after ArduinoHA has
  // published its own discovery messages.
  mqtt.subscribe("aha/a55cdbdf7b37/+/set_pos_t");
  customDiscoveryPending = true;
  customDiscoveryAt = millis() + 1500;
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
          startMovement(i, true);
        }
        break;
      }

      case HACover::CommandClose: {
        Serial.print("HA -> CLOSE -> ");
        Serial.println(shutterControl.name(id));

        ok = shutterControl.close(id);

        if (ok) {
          startMovement(i, false);
        }
        break;
      }

      case HACover::CommandStop: {
        Serial.print("HA -> STOP -> ");
        Serial.println(shutterControl.name(id));

        updateMovement(i);
        const int stopPosition = getKnownOrAssumedPosition(sender, false);

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

  // Position is estimated from the known motor travel time.
  // We publish the state/position ourselves, so optimistic mode is disabled.
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
  mqtt.onMessage(onMqttMessage);

  // ArduinoHA's default MQTT buffer is 256 bytes. Our custom
  // discovery messages are larger, so use a safe buffer size.
  mqtt.setBufferSize(1024);

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
  Serial.println("Positionsberechnung: zeitbasiert (27s AUF / 17s ZU)");
  Serial.println("================================");
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  mqtt.loop();

  if (
    customDiscoveryPending &&
    mqtt.isConnected() &&
    (int32_t)(millis() - customDiscoveryAt) >= 0
  ) {
    customDiscoveryPending = false;
    publishAllCoverDiscovery();
  }

  updateAllMovements();

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
