#include "ELECHOUSE_CC1101_SRC_DRV.h"
#define PIN_RECEIVE 33
#define PIN_SEND 32

class ShutterControl {
  private:
    // each pulse has one of these lengths
    int pulses[6] = {4760, -1400, 560, -280, 280, -840};
    // total command will be repeated 10 times
    int repeat = 10;
    // each command is created from startChars + shutter device id + command (open/close/stop)
    char startChars[3] = "01";
    char commandToSend[85];
    char commandClose[16] = "545232345452323";
    char commandOpen[16] = "545452345454523";
    char commandStop[16] = "523452345234523";
    
    // take the command, convert each number into a pulse and send it
    void sendCommand(char cmd[]) { 
      int cmdLength = strlen(cmd);
      int cmdParts[cmdLength];
      for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < cmdLength; i++) {
            char c = cmd[i];
            int index = c - '0';
            cmdParts[i] = pulses[index];
          }
        int delay = 0;
        unsigned long time;
        byte n = 0;

        for (int i = 0; i < cmdLength; i++) {
          n = 1;
          delay = cmdParts[i];
          if (delay < 0) {
            delay = delay * -1;
            n = 0;
          }

          digitalWrite(PIN_SEND, n);
          delayMicroseconds(delay);
        }
        digitalWrite(PIN_SEND, 0);
      }
    }

  
  public:
    void setup() {
      ELECHOUSE_cc1101.Init();
      ELECHOUSE_cc1101.setGDO(PIN_SEND, PIN_RECEIVE);
      ELECHOUSE_cc1101.setMHZ(433.92);
      ELECHOUSE_cc1101.SetTx();
      ELECHOUSE_cc1101.setModulation(2);
      ELECHOUSE_cc1101.setDRate(512);
      ELECHOUSE_cc1101.setPktFormat(3);

      if (!ELECHOUSE_cc1101.getCC1101()) {
        Serial.println("CC1101 Connection Error");
      }
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