#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#define W5500_CS    14
#define W5500_MISO  12
#define W5500_MOSI  11
#define W5500_SCK   13

byte mac[] = {
  0xA5, 0x5C, 0xDB, 0xDF, 0x7B, 0x37
};

// Test-IP
IPAddress ip(192, 168, 188, 250);
IPAddress dns(192, 168, 188, 1);
IPAddress gateway(192, 168, 188, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("WAVESHARE ETHERNET STATIC TEST");
  Serial.println("==============================");

  SPI.begin(
    W5500_SCK,
    W5500_MISO,
    W5500_MOSI,
    W5500_CS
  );

  Ethernet.init(W5500_CS);

  Serial.println("Starte W5500 mit statischer IP...");

  Ethernet.begin(mac, ip, dns, gateway, subnet);

  delay(1000);

  Serial.print("Hardware: ");

  if (Ethernet.hardwareStatus() == EthernetW5500) {
    Serial.println("W5500 GEFUNDEN");
  } else {
    Serial.println("W5500 NICHT GEFUNDEN");
  }

  Serial.print("Link: ");

  if (Ethernet.linkStatus() == LinkON) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  Serial.print("IP-Adresse: ");
  Serial.println(Ethernet.localIP());

  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());

  Serial.println("Test beendet.");
}

void loop() {
  Serial.print("Link: ");

  if (Ethernet.linkStatus() == LinkON) {
    Serial.print("ON  ");
  } else {
    Serial.print("OFF ");
  }

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  delay(2000);
}
