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
// Eigener SPI-Bus für CC1101
// W5500 läuft separat auf SPI3_HOST
// ============================================================

SPIClass cc1101SPI(HSPI);

SPISettings cc1101SPISettings(
  2000000,
  MSBFIRST,
  SPI_MODE0
);

// RadioLib-Modul
//
// CS   = GPIO21
// GDO0 = GPIO3
// RST  = nicht vorhanden
// GDO2 = GPIO2
//
CC1101 radio = new Module(
  CC1101_CS,
  PIN_SEND,
  RADIOLIB_NC,
  PIN_RECEIVE,
  cc1101SPI,
  cc1101SPISettings
);


// ============================================================
// SHUTTER CONTROL
// ============================================================

class ShutterControl {

  private:

    // Puls-Längen
    int pulses[6] = {
      4760,
      -1400,
      560,
      -280,
      280,
      -840
    };

    // Befehl 10x wiederholen
    int repeat = 10;

    // Start
    char startChars[3] = "01";

    // Sende-Puffer
    char commandToSend[85];

    // Rollladen-Befehle
    char commandClose[16] = "545232345452323";
    char commandOpen[16]  = "545452345454523";
    char commandStop[16]  = "523452345234523";


    // ========================================================
    // Befehl senden
    // ========================================================

    void sendCommand(char cmd[]) {

      int cmdLength = strlen(cmd);

      int cmdParts[cmdLength];

      for (int r = 0; r < repeat; r++) {

        for (int i = 0; i < cmdLength; i++) {

          char c = cmd[i];

          int index = c - '0';

          cmdParts[i] = pulses[index];
        }

        int pulseDelay = 0;
        byte n = 0;

        for (int i = 0; i < cmdLength; i++) {

          n = 1;

          pulseDelay = cmdParts[i];

          if (pulseDelay < 0) {
            pulseDelay = -pulseDelay;
            n = 0;
          }

          digitalWrite(PIN_SEND, n);

          delayMicroseconds(pulseDelay);
        }

        digitalWrite(PIN_SEND, LOW);
      }
    }


  public:

    // ========================================================
    // CC1101 SETUP
    // ========================================================

    void setup() {

      Serial.println();
      Serial.println("----- CC1101 RADIOLIB SETUP -----");


      // ------------------------------------------------------
      // GDO Pins
      // ------------------------------------------------------

      pinMode(PIN_SEND, OUTPUT);
      digitalWrite(PIN_SEND, LOW);

      pinMode(PIN_RECEIVE, INPUT);

      Serial.println("GDO Pins OK");


      // ------------------------------------------------------
      // Eigenen SPI-Bus starten
      // ------------------------------------------------------

      Serial.println("Starte CC1101 SPI-Bus...");

      cc1101SPI.begin(
        CC1101_SCK,
        CC1101_MISO,
        CC1101_MOSI,
        CC1101_CS
      );

      Serial.println("CC1101 SPI-Bus gestartet");


      // ------------------------------------------------------
      // CC1101 initialisieren
      // ------------------------------------------------------

      Serial.println("Starte RadioLib CC1101...");

      int16_t state = radio.begin(
        433.92,     // Frequenz MHz
        4.8,        // zunächst normale RadioLib-Bitrate
        5.0,        // Frequenzabweichung
        135.0,      // RX-Bandbreite
        10,         // Sendeleistung
        16          // Preamble
      );

      Serial.print("RadioLib Init Status: ");
      Serial.println(state);


      if (state == RADIOLIB_ERR_NONE) {

        Serial.println("CC1101: GEFUNDEN");

      } else {

        Serial.print("CC1101 INIT FEHLER: ");
        Serial.println(state);

        Serial.println("CC1101 konnte nicht initialisiert werden.");

        // Kein Reset / kein Endlos-Hängen.
        // Das restliche Gerät darf weiterlaufen.
        return;
      }


      // ------------------------------------------------------
      // OOK aktivieren
      // ------------------------------------------------------

      Serial.println("Aktiviere OOK...");

      state = radio.setOOK(true);

      Serial.print("OOK Status: ");
      Serial.println(state);


      // ------------------------------------------------------
      // Promiscuous Mode
      // ------------------------------------------------------

      Serial.println("Aktiviere Promiscuous Mode...");

      state = radio.setPromiscuousMode(true);

      Serial.print("Promiscuous Status: ");
      Serial.println(state);


      Serial.println("----- CC1101 RADIOLIB SETUP ENDE -----");
    }


    // ========================================================
    // ÖFFNEN
    // ========================================================

    void openShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);

      strcat(commandToSend, deviceId);

      strcat(commandToSend, commandOpen);

      sendCommand(commandToSend);
    }


    // ========================================================
    // SCHLIESSEN
    // ========================================================

    void closeShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);

      strcat(commandToSend, deviceId);

      strcat(commandToSend, commandClose);

      sendCommand(commandToSend);
    }


    // ========================================================
    // STOP
    // ========================================================

    void stopShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);

      strcat(commandToSend, deviceId);

      strcat(commandToSend, commandStop);

      sendCommand(commandToSend);
    }
};
