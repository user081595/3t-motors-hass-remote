#define ESP32 1

#include <Arduino.h>
#include <SPI.h>
#include "ELECHOUSE_CC1101_SRC_DRV.h"

#define PIN_RECEIVE 2
#define PIN_SEND    3

#define CC1101_SCK  18
#define CC1101_MISO 16
#define CC1101_MOSI 17
#define CC1101_CS   21

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


    // ==========================================
    // Befehl senden
    // ==========================================

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

    // ==========================================
    // CC1101 Setup
    // ==========================================

    void setup() {

      Serial.println();
      Serial.println("----- CC1101 SETUP -----");


      // ------------------------------------------
      // GDO-Pins
      // ------------------------------------------

      pinMode(PIN_SEND, OUTPUT);

      digitalWrite(PIN_SEND, LOW);

      pinMode(PIN_RECEIVE, INPUT);

      Serial.println("GDO Pins OK");


      // ------------------------------------------
      // CC1101 SPI-Pins
      // ------------------------------------------

      Serial.println("Setze CC1101 SPI-Pins...");

      ELECHOUSE_cc1101.setSpiPin(
        CC1101_SCK,
        CC1101_MISO,
        CC1101_MOSI,
        CC1101_CS
      );

      Serial.println("setSpiPin() OK");


      // ------------------------------------------
      // CS vorbereiten
      // ------------------------------------------

      pinMode(CC1101_CS, OUTPUT);

      digitalWrite(CC1101_CS, HIGH);

      Serial.println("CS HIGH");


      // ------------------------------------------
      // Wichtig:
      // Globalen Arduino-SPI-Bus vor Init
      // sauber beenden.
      //
      // Der W5500 läuft auf SPI3_HOST.
      // Der CC1101 verwendet den globalen
      // Arduino SPI-Bus.
      // ------------------------------------------

      Serial.println("Beende globalen SPI-Bus...");

      SPI.end();

      delay(20);

      Serial.println("SPI.end() OK");


      // ------------------------------------------
      // CC1101 initialisieren
      // ------------------------------------------

      Serial.println("Starte CC1101 Init()...");

      ELECHOUSE_cc1101.Init();

      Serial.println("CC1101 Init() beendet!");


      // ------------------------------------------
      // GDO
      // ------------------------------------------

      Serial.println("Setze GDO...");

      ELECHOUSE_cc1101.setGDO(
        PIN_SEND,
        PIN_RECEIVE
      );

      Serial.println("GDO gesetzt");


      // ------------------------------------------
      // Frequenz
      // ------------------------------------------

      Serial.println("Setze Frequenz 433.92 MHz...");

      ELECHOUSE_cc1101.setMHZ(433.92);

      Serial.println("Frequenz gesetzt");


      // ------------------------------------------
      // TX
      // ------------------------------------------

      Serial.println("Setze TX...");

      ELECHOUSE_cc1101.SetTx();

      Serial.println("TX gesetzt");


      // ------------------------------------------
      // Modulation
      // ------------------------------------------

      Serial.println("Setze Modulation...");

      ELECHOUSE_cc1101.setModulation(2);

      Serial.println("Modulation gesetzt");


      // ------------------------------------------
      // Datenrate
      // ------------------------------------------

      Serial.println("Setze Datenrate...");

      ELECHOUSE_cc1101.setDRate(512);

      Serial.println("Datenrate gesetzt");


      // ------------------------------------------
      // Packet Format
      // ------------------------------------------

      Serial.println("Setze Packet Format...");

      ELECHOUSE_cc1101.setPktFormat(3);

      Serial.println("Packet Format gesetzt");


      // ------------------------------------------
      // Verbindung prüfen
      // ------------------------------------------

      Serial.println("Prüfe CC1101 Verbindung...");

      if (ELECHOUSE_cc1101.getCC1101()) {

        Serial.println("CC1101: GEFUNDEN");

      } else {

        Serial.println("CC1101: NICHT GEFUNDEN");
      }


      Serial.println("----- CC1101 SETUP ENDE -----");
    }


    // ==========================================
    // Öffnen
    // ==========================================

    void openShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);

      strcat(commandToSend, deviceId);

      strcat(commandToSend, commandOpen);

      sendCommand(commandToSend);
    }


    // ==========================================
    // Schließen
    // ==========================================

    void closeShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);

      strcat(commandToSend, deviceId);

      strcat(commandToSend, commandClose);

      sendCommand(commandToSend);
    }


    // ==========================================
    // Stop
    // ==========================================

    void stopShutter(char deviceId[66]) {

      strcpy(commandToSend, startChars);

      strcat(commandToSend, deviceId);

      strcat(commandToSend, commandStop);

      sendCommand(commandToSend);
    }
};
