#include <Arduino.h>

void setup() {
  Serial.begin(115200);

  delay(3000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-S3 STARTET");
  Serial.println("USB CDC funktioniert!");
  Serial.println("================================");
}

void loop() {
  Serial.println("Läuft...");
  delay(2000);
}
