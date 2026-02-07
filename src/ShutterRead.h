#include "ELECHOUSE_CC1101_SRC_DRV.h"
#include <map>
#include <climits>
#include <cmath>

#define PIN_RECEIVE 33
#define TIMINGS_LEN 1024
#define SHUTTER_ID_LEN 65

static int interruptPin = 0;
float freq = 433.92;
float rxbw = 325;

static unsigned long lastTime = 0;
static unsigned long resetTime;
bool isReceiving = false;
static unsigned int timings[TIMINGS_LEN];

int counter = 0;
int countThreshold = 50;
int timeThreshold = 70;

// when a signal is recorded and a device is detected, the detected device signal will be stored in here
static char recordedShutterId[66];

// signals that can be received. Similar signals will be mapped to the closest one
std::map<int, int> signals = {
    {4760, 0},
    {-1400, 1},
    {560, 2},
    {-280, 3},
    {280, 4},
    {-840, 5},
    {840, 2},
    {-560, 5}};

void stopRecording()
{
  isReceiving = false;
  detachInterrupt(interruptPin);
}

// get the value closest to any of the signals
int nearestValue(const std::map<int, int> &signals, int input)
{
  int result = 0;
  int bestDiff = INT_MAX;
  for (const auto &[key, value] : signals)
  {
    int diff = std::abs(input - key);
    if (diff < bestDiff)
    {
      bestDiff = diff;
      result = value;
    }
  }
  return result;
}

// search for a start signals (01) in the received signals, then return the next 65 signals
bool extractShutterIdFromTimings()
{
  // smoothen all the values first
  for (size_t i = 0; i < TIMINGS_LEN; ++i)
  {
    timings[i] = nearestValue(signals, (int)timings[i]);
  }
  // iterate, but only as long as the shutter id would fit
  for (size_t i = 0; i + SHUTTER_ID_LEN + 1 < TIMINGS_LEN; i++)
  {
    // 0 followed by a 1 is found, the shutter id will follow
    if (timings[i] == 0 && timings[i + 1] == 1)
    {
      size_t k = 0;
      // add the 65 signals after the start signal to the result
      for (size_t j = i + 2; j < i + 2 + SHUTTER_ID_LEN; j++)
      {
        recordedShutterId[k++] = static_cast<char>('0' + timings[j]);
      }
      recordedShutterId[k] = '\0';
      return true;
    }
  }
  return false; // no start signals found found
}

void handleInterrupt()
{
  static unsigned long lastTime = 0;
  const long time = micros();
  const unsigned int duration = time - lastTime;

  // try to detect a signal
  if (millis() - resetTime > timeThreshold)
  {
    if (counter > countThreshold)
    {
      isReceiving = true;
      detachInterrupt(interruptPin);
    }
    else
    {
      counter = 0;
      isReceiving = false;
    }
  }
  // wait for first pulse with a length > 4700
  if (counter == 0 && duration >= 4700 || counter > 0 && duration > 100)
  {
    // alternate high/low signals
    if (counter % 2 == 0) {
      timings[counter] = duration * -1;
    } else {
      timings[counter] = duration;
    }
    counter++;
  }

  if (counter >= TIMINGS_LEN)
  {
    isReceiving = true;
    detachInterrupt(interruptPin);
  }

  lastTime = time;
  resetTime = millis();
}

// setup receiving
void startReceive()
{
  pinMode(PIN_RECEIVE, INPUT);
  interruptPin = digitalPinToInterrupt(PIN_RECEIVE);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setRxBW(rxbw);
  ELECHOUSE_cc1101.setMHZ(freq);
  ELECHOUSE_cc1101.SetRx();
  isReceiving = false;
  counter = 0;
  attachInterrupt(interruptPin, handleInterrupt, CHANGE);
}