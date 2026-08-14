#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ============================================================
// CC1101 PINS
// ============================================================

#define PIN_SEND 3

#define CC1101_SCK  18
#define CC1101_MISO 16
#define CC1101_MOSI 17
#define CC1101_CS   21


// ============================================================
// EIGENER SPI-BUS FÜR CC1101
// W5500 läuft separat auf SPI3_HOST
// ============================================================

static SPIClass cc1101SPI(FSPI);

static SPISettings cc1101SPISettings(
  2000000,
  MSBFIRST,
  SPI_MODE0
);


// ============================================================
// RADIO LIB CC1101
//
// CS   = GPIO21
// GDO0 = GPIO3
// RST  = nicht vorhanden
// GDO2 = nicht benötigt
// ============================================================

static Module cc1101Module(
  CC1101_CS,
  PIN_SEND,
  RADIOLIB_NC,
  RADIOLIB_NC,
  cc1101SPI,
  cc1101SPISettings
);

static CC1101 radio(&cc1101Module);


// ============================================================
// SHUTTER CONTROL
// ============================================================

class ShutterControl {

  private:

    // --------------------------------------------------------
    // Deine ursprünglichen Pulszeiten
    // --------------------------------------------------------

    int pulses[6] = {
      4760,
      -1400,
      560,
      -280,
      280,
      -840
    };


    // --------------------------------------------------------
    // Befehl 10x wiederholen
    // --------------------------------------------------------

    int repeat = 10;


    // --------------------------------------------------------
    // Startzeichen
    // --------------------------------------------------------

    char startChars[3] = "01";


    // --------------------------------------------------------
    // Sendepuffer
    // --------------------------------------------------------

    char commandToSend[85];


    // --------------------------------------------------------
    // Deine ursprünglichen Befehle
    // --------------------------------------------------------

    char commandClose[16] = "545232345452323";

    char commandOpen[16]  = "545452345454523";

    char commandStop[16]  = "523452345234523";


    // ========================================================
    // RAW SIGNAL SENDEN
    // ========================================================

    void sendCommand(char cmd[]) {

      int cmdLength = strlen(cmd);

      Serial.println();
      Serial.println("--------------------------------");
      Serial.println("CC1101 SENDUNG");
      Serial.println("--------------------------------");

      Serial.print("Command: ");
      Serial.println(cmd);

      Serial.print("Länge: ");
      Serial.println(cmdLength);

      // ------------------------------------------------------
      // Direkten TX-Modus starten
      // ------------------------------------------------------

      int16_t state = radio.transmitDirectAsync();

      Serial.print("transmitDirectAsync(): ");
      Serial.println(state);

      if (state != RADIOLIB_ERR_NONE) {

        Serial.println("CC1101 TX START FEHLER!");

        return;
      }


      // GDO0 wird jetzt vom ESP32 als Dateneingang
      // für den CC1101 verwendet.

      pinMode(PIN_SEND, OUTPUT);

      digitalWrite(PIN_SEND, LOW);

      delayMicroseconds(100);


      // ------------------------------------------------------
      // Command in Pulszeiten umwandeln und senden
      // ------------------------------------------------------

      int cmdParts[cmdLength];

      for (int i = 0; i < cmdLength; i++) {

        char c = cmd[i];

        int index = c - '0';

        if (index < 0 || index > 5) {

          Serial.print("Ungültiges Command-Zeichen: ");
          Serial.println(c);

          digitalWrite(PIN_SEND, LOW);

          radio.packetMode();

          return;
        }

        cmdParts[i] = pulses[index];
      }


      // ------------------------------------------------------
      // 10 Wiederholungen
      // ------------------------------------------------------

      for (int r = 0; r < repeat; r++) {

        for (int i = 0; i < cmdLength; i++) {

          int pulseDelay = cmdParts[i];

          bool level = HIGH;

          if (pulseDelay < 0) {

            pulseDelay = -pulseDelay;

            level = LOW;
          }

          digitalWrite(
            PIN_SEND,
            level ? HIGH : LOW
          );

          delayMicroseconds(
            pulseDelay
          );
        }
      }


      // ------------------------------------------------------
      // Ausgang abschalten
      // ------------------------------------------------------

      digitalWrite(
        PIN_SEND,
        LOW
      );

      delayMicroseconds(100);


      // ------------------------------------------------------
      // Direct Mode beenden
      // ------------------------------------------------------

      radio.packetMode();

      Serial.println("Sendung beendet.");
      Serial.println("--------------------------------");
    }


  public:


    // ========================================================
    // CC1101 SETUP
    // ========================================================

    void setup() {

      Serial.println();
      Serial.println("----- CC1101 RADIOLIB TX SETUP -----");


      // ------------------------------------------------------
      // GDO0
      // ------------------------------------------------------

      pinMode(
        PIN_SEND,
        OUTPUT
      );

      digitalWrite(
        PIN_SEND,
        LOW
      );

      Serial.println(
        "GDO0 = GPIO3"
      );


      // ------------------------------------------------------
      // Eigenen SPI-Bus starten
      // ------------------------------------------------------

      Serial.println(
        "Starte CC1101 SPI-Bus..."
      );

      cc1101SPI.begin(
        CC1101_SCK,
        CC1101_MISO,
        CC1101_MOSI,
        CC1101_CS
      );

      Serial.println(
        "CC1101 SPI-Bus gestartet"
      );


      // ------------------------------------------------------
      // CC1101 initialisieren
      // ------------------------------------------------------

      Serial.println(
        "Starte RadioLib CC1101..."
      );

      int16_t state = radio.begin(
        433.92,   // MHz
        512.0,    // kbit/s
        5.0,      // Frequenzabweichung
        650.0,    // RX-Bandbreite
        10,       // TX-Leistung
        16        // Preamble
      );

      Serial.print(
        "RadioLib Init Status: "
      );

      Serial.println(state);


      if (state != RADIOLIB_ERR_NONE) {

        Serial.print(
          "CC1101 INIT FEHLER: "
        );

        Serial.println(state);

        return;
      }


      Serial.println(
        "CC1101: GEFUNDEN"
      );


      // ------------------------------------------------------
      // OOK
      // ------------------------------------------------------

      Serial.println(
        "Aktiviere OOK..."
      );

      state = radio.setOOK(true);

      Serial.print(
        "OOK Status: "
      );

      Serial.println(state);


      // ------------------------------------------------------
      // TX-Leistung
      // ------------------------------------------------------

      state = radio.setOutputPower(10);

      Serial.print(
        "TX Power Status: "
      );

      Serial.println(state);


      Serial.println();
      Serial.println(
        "CC1101 ist bereit zum SENDEN."
      );

      Serial.println(
        "----- CC1101 RADIOLIB TX SETUP ENDE -----"
      );
    }


    // ========================================================
    // OPEN
    // ========================================================

    void openShutter(char deviceId[66]) {

      strcpy(
        commandToSend,
        startChars
      );

      strcat(
        commandToSend,
        deviceId
      );

      strcat(
        commandToSend,
        commandOpen
      );

      Serial.println(
        "Rollladen: OPEN"
      );

      sendCommand(
        commandToSend
      );
    }


    // ========================================================
    // CLOSE
    // ========================================================

    void closeShutter(char deviceId[66]) {

      strcpy(
        commandToSend,
        startChars
      );

      strcat(
        commandToSend,
        deviceId
      );

      strcat(
        commandToSend,
        commandClose
      );

      Serial.println(
        "Rollladen: CLOSE"
      );

      sendCommand(
        commandToSend
      );
    }


    // ========================================================
    // STOP
    // ========================================================

    void stopShutter(char deviceId[66]) {

      strcpy(
        commandToSend,
        startChars
      );

      strcat(
        commandToSend,
        deviceId
      );

      strcat(
        commandToSend,
        commandStop
      );

      Serial.println(
        "Rollladen: STOP"
      );

      sendCommand(
        commandToSend
      );
    }
};
