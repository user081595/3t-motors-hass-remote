#include <Arduino.h>
#include <ArduinoHA.h>
#include <ETH.h>
#include <Network.h>
#include <config.h>
#include <ShutterControl.h>

Config config;

// ==============================
// W5500
// ==============================

#define ETH_PHY_TYPE   ETH_PHY_W5500
#define ETH_PHY_ADDR   1

#define ETH_PHY_CS     14
#define ETH_PHY_IRQ    10
#define ETH_PHY_RST     9

#define ETH_SPI_SCK    13
#define ETH_SPI_MISO   12
#define ETH_SPI_MOSI   11

// ==============================
// Netzwerk
// ==============================

IPAddress local_ip(192, 168, 188, 250);
IPAddress gateway(192, 168, 188, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 188, 1);

bool ethConnected = false;

// ==============================
// MQTT / Home Assistant
// ==============================

NetworkClient client;

byte mac[] = {
  0xA5, 0x5C, 0xDB, 0xDF, 0x7B, 0x37
};

HADevice device(mac, sizeof(mac));
HAMqtt mqtt(client, device);

HASensor statusSensor("status");

// ==============================
// CC1101 / Shutter
// ==============================

ShutterControl shutterControl;

// ==============================
// Ethernet Events
// ==============================

void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info) {

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

      Serial.print("IP: ");
      Serial.println(ETH.localIP());

      Serial.print("Gateway: ");
      Serial.println(ETH.gatewayIP());

      Serial.print("Subnet: ");
      Serial.println(ETH.subnetMask());

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

// ==============================
// Setup
// ==============================

void setup() {

  Serial.begin(115200);

  delay(3000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-S3 ROLLLADEN CONTROLLER");
  Serial.println("Ethernet + MQTT + CC1101");
  Serial.println("================================");

  // ==============================
  // Ethernet starten
  // ==============================

  Network.onEvent(onNetworkEvent);

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

  // ==============================
  // Statische IP
  // ==============================

  if (ETH.config(local_ip, gateway, subnet, dns)) {
    Serial.println("Statische IP: OK");
  } else {
    Serial.println("Statische IP: FEHLER");
  }

  // ==============================
  // Auf Ethernet warten
  // ==============================

  Serial.println("Warte auf Ethernet...");

  unsigned long start = millis();

  while (!ethConnected && millis() - start < 15000) {
    delay(100);
  }

  if (!ethConnected) {
    Serial.println("Ethernet nicht verbunden!");
    return;
  }

  Serial.println("Ethernet ist bereit.");

  // ==============================
  // Home Assistant
  // ==============================

  device.setName("ESP32-S3 Rollladen Controller");
  device.setSoftwareVersion("1.0.0");

  statusSensor.setName("Status");
  statusSensor.setIcon("mdi:lan-connect");

  // ==============================
  // MQTT
  // ==============================

  Serial.println();
  Serial.println("Starte MQTT...");

  mqtt.begin(
    config.mqttConfig.host.c_str(),
    config.mqttConfig.port,
    config.mqttConfig.username.c_str(),
    config.mqttConfig.password.c_str()
  );

  Serial.println("MQTT gestartet");

  // ==============================
  // CC1101
  // ==============================

  Serial.println();
  Serial.println("================================");
  Serial.println("STARTE CC1101");
  Serial.println("================================");

  Serial.println("SPI:");
  Serial.println("SCK  = GPIO18");
  Serial.println("MISO = GPIO16");
  Serial.println("MOSI = GPIO17");
  Serial.println("CS   = GPIO21");
  Serial.println("GDO0 = GPIO3");
  Serial.println("GDO2 = GPIO2");

  // Genau hier prüfen wir jetzt,
  // ob setup() des CC1101 hängen bleibt.

  Serial.println();
  Serial.println("Rufe shutterControl.setup() auf...");

  shutterControl.setup();

  Serial.println("shutterControl.setup() beendet.");

  Serial.println("CC1101 Setup beendet.");

  // ==============================
  // Fertig
  // ==============================

  Serial.println();
  Serial.println("================================");
  Serial.println("SETUP FERTIG");
  Serial.println("================================");
}

// ==============================
// Loop
// ==============================

void loop() {

  mqtt.loop();

  // Status regelmäßig veröffentlichen
  static unsigned long lastStatus = 0;

  if (millis() - lastStatus >= 5000) {

    lastStatus = millis();

    statusSensor.setValue("online");

    Serial.println("MQTT Status -> online");
  }

  delay(10);
}
