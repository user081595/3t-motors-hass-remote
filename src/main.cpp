#include <Arduino.h>
#include <ETH.h>
#include <Network.h>

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

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {

  switch (event) {

    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("esp32-s3-shutter");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("ETH Got IP!");
      Serial.print("IP: ");
      Serial.println(ETH.localIP());
      Serial.print("Gateway: ");
      Serial.println(ETH.gatewayIP());
      Serial.print("Subnet: ");
      Serial.println(ETH.subnetMask());
      ethConnected = true;
      break;

    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("ETH Lost IP");
      ethConnected = false;
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      ethConnected = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      ethConnected = false;
      break;

    default:
      break;
  }
}

void setup() {

  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32-S3 W5500 NATIVE TEST");
  Serial.println("==============================");

  Network.onEvent(onEvent);

  Serial.println("Starte W5500...");

  bool result = ETH.begin(
    ETH_PHY_TYPE,
    ETH_PHY_ADDR,
    ETH_PHY_CS,
    ETH_PHY_IRQ,
    ETH_PHY_RST,
    SPI2_HOST,
    ETH_SPI_SCK,
    ETH_SPI_MISO,
    ETH_SPI_MOSI
  );

  Serial.print("ETH.begin(): ");
  Serial.println(result ? "OK" : "FEHLER");

  delay(1000);

  Serial.println("Setze statische IP...");

  if (ETH.config(local_ip, gateway, subnet, dns)) {
    Serial.println("ETH.config(): OK");
  } else {
    Serial.println("ETH.config(): FEHLER");
  }
}

void loop() {

  Serial.print("Link: ");

  if (ETH.linkUp()) {
    Serial.print("ON");
  } else {
    Serial.print("OFF");
  }

  Serial.print(" | IP: ");
  Serial.println(ETH.localIP());

  delay(2000);
}
