#include <SPI.h>
#include <Ethernet.h>

#define W5500_CS    14
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
  Serial.println("WAVESHARE ETHERNET TEST");
  Serial.println("==============================");

  SPI.begin(
    W5500_SCK,
    W5500_MISO,
    W5500_MOSI,
    W5500_CS
  );

  Ethernet.init(W5500_CS);

  Serial.println("W5500 wird gestartet...");

  Serial.print("Hardware: ");

  if (Ethernet.hardwareStatus() == EthernetW5500) {
    Serial.println("W5500 GEFUNDEN");
  } else {
    Serial.println("W5500 NICHT GEFUNDEN");
  }

  Serial.print("LAN-Link: ");

  if (Ethernet.linkStatus() == LinkON) {
    Serial.println("ON");
  } else if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("OFF");
  } else {
    Serial.println("UNKNOWN");
  }

  Serial.println("Starte DHCP...");

  int result = Ethernet.begin(mac);

  if (result == 0) {
    Serial.println("DHCP fehlgeschlagen!");
  } else {
    Serial.println("DHCP erfolgreich!");
  }

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  Serial.print("Link: ");

  if (Ethernet.linkStatus() == LinkON) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  delay(2000);
}
