#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#define W5500_CS    14
#define W5500_RST    9
#define W5500_INT   10
#define W5500_MISO  12
#define W5500_MOSI  11
#define W5500_SCK   13

byte mac[] = {
  0xA5, 0x5C, 0xDB, 0xDF, 0x7B, 0x37
};

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32-S3 ETH TEST");
  Serial.println("==============================");

  // W5500 SPI
  SPI.begin(
    W5500_SCK,
    W5500_MISO,
    W5500_MOSI,
    W5500_CS
  );

  Ethernet.init(W5500_CS);

  Serial.println("Starte Ethernet über DHCP...");

  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP fehlgeschlagen!");
    Serial.println("Ethernet-Link trotzdem prüfen.");

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("W5500 nicht gefunden!");
    } else {
      Serial.println("W5500 gefunden.");
    }
  } else {
    Serial.println("Ethernet verbunden!");
    Serial.print("IP-Adresse: ");
    Serial.println(Ethernet.localIP());

    Serial.print("Gateway: ");
    Serial.println(Ethernet.gatewayIP());

    Serial.print("Subnetz: ");
    Serial.println(Ethernet.subnetMask());
  }
}

void loop() {
  Ethernet.maintain();

  Serial.print("Link: ");

  if (Ethernet.linkStatus() == LinkON) {
    Serial.println("ON");
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("OFF");
  } else {
    Serial.println("UNKNOWN");
  }

  delay(2000);
}
