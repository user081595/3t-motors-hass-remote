#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ============================================================
// CC1101 PINS
// ============================================================

#define PIN_RECEIVE 2
#define PIN_SEND    3

#define CC1101_SCK  18
#define CC1101_MISO 16
#define CC1101_MOSI 17
#define CC1101_CS   21


// ============================================================
// EIGENER SPI-BUS FÜR CC1101
// ============================================================

static SPIClass cc1101SPI(FSPI);

static SPISettings cc1101SPISettings(
  2000000,
  MSBFIRST,
  SPI_MODE0
);


// ============================================================
// RADIO LIB CC1101
// ============================================================

static Module cc1101Module(
  CC1101_CS,
  PIN_SEND,
  RADIOLIB_NC,
  PIN_RECEIVE,
  cc1101SPI,
  cc1101SPISettings
);

static CC1101 radio(&cc1101Module);


// ============================================================
// SHUTTER CONTROL
// ============================================================

class ShutterControl {

  private:

    int pulses[6] = {
      4760,
      -1400,
      560,
      -280,
      280,
      -840
    };

    int repeat = 10;

    char startChars[3] = "01";

    char commandToSend[85];

    char commandClose[16] = "545232345452323";
    char commandOpen[16]  = "545452345454523";
    char commandStop[16]  = "523452345234523";


    void sendCommand(char cmd[]) {

      int cmdLength = strlen(cmd);

      int cmdParts[cmdLength];

      for (int r = 0; r < repeat; r++) {

        for (int i = 0; i < cmdLength; i++) {

          int index = cmd[i] - '0';

          if (index < 0 || index > 5) {
            return;
          }

          cmdParts[i] = pulses[index];
        }

        for (int i = 0; i < cmdLength; i++) {

          int pulseDelay = cmdParts[i];

          byte level = HIGH;

          if (pulseDelay < 0) {
            pulseDelay = -pulseDelay;
            level = LOW;
          }

          digitalWrite(PIN_SEND, level);
          delayMicroseconds(pulseDelay);
        }

        digitalWrite(PIN_SEND, LOW);
      }
    }


  public:

    void setup() {

      Serial.println();
      Serial.println("----- CC1101 RADIOLIB SETUP -----");


      // GDO-Pins
      pinMode(PIN_SEND, OUTPUT);
      digitalWrite(PIN_SEND, LOW);

      pinMode(PIN_RECEIVE, INPUT);

      Serial.println("GDO Pins OK");


      // Eigener SPI-Bus
      Serial.println("Starte CC1101 SPI-Bus...");

      cc1101SPI.begin(
        CC1101_SCK,
        CC1101_MISO,
        CC1101_MOSI,
        CC1101_CS
      );

      Serial.println("CC1101 SPI-Bus gestartet");


      // CC1101
      Serial.println("Starte RadioLib CC1101...");

      int16_t state = radio.begin(
        433.92,
        4.8,
        5.0,
        135.0,
        10,
        16
      );

      Serial.print("RadioLib Init Status: ");
      Serial.println(state);

      if (state != RADIOLIB_ERR_NONE) {

        Serial.print("CC1101 INIT FEHLER: ");
        Serial.println(state);

        return;
      }

      Serial.println("CC1101: GEFUNDEN");


      // OOK
      Serial.println("Aktiviere OOK...");

      state = radio.setOOK(true);

      Serial.print("OOK Status: ");
      Serial.println(state);


      // Promiscuous Mode
      Serial.println("Aktiviere Promiscuous Mode...");

      state = radio.setPromiscuousMode(true);

      Serial.print("Promiscuous Status: ");
      Serial.println(state);


      Serial.println("----- CC1101 RADIOLIB SETUP ENDE -----");
    }


    void openShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);
      strcat(commandToSend, deviceId);
      strcat(commandToSend, commandOpen);

      sendCommand(commandToSend);
    }


    void closeShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);
      strcat(commandToSend, deviceId);
      strcat(commandToSend, commandClose);

      sendCommand(commandToSend);
    }


    void stopShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);
      strcat(commandToSend, deviceId);
      strcat(commandToSend, commandStop);

      sendCommand(commandToSend);
    }
};
