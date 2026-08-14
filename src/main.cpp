#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#define W5500_CS   14
#define W5500_RST   9
#define W5500_INT  10
#define W5500_MISO 12
#define W5500_MOSI 11
#define W5500_SCK  13

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("W5500 HARDWARE TEST V2");
  Serial.println("==============================");

  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, LOW);
  delay(100);
  digitalWrite(W5500_RST, HIGH);
  delay(200);

  SPI.begin(
    W5500_SCK,
    W5500_MISO,
    W5500_MOSI,
    W5500_CS
  );

  Ethernet.init(W5500_CS);

  Serial.println("Prüfe W5500...");

  if (Ethernet.hardwareStatus() == EthernetW5500) {
    Serial.println("W5500: GEFUNDEN");
  } else {
    Serial.println("W5500: NICHT GEFUNDEN");
  }

  Serial.println("Prüfe LAN-Link...");

  EthernetLinkStatus link = Ethernet.linkStatus();

  if (link == LinkON) {
    Serial.println("LAN-Link: ON");
  } else if (link == LinkOFF) {
    Serial.println("LAN-Link: OFF");
  } else {
    Serial.println("LAN-Link: UNKNOWN");
  }

  Serial.println("Test beendet.");
}

void loop() {
  Serial.println("ESP läuft...");
  delay(2000);
}
