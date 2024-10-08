

#include "SLIPEncodedTCP.h"
#include "config.h"
#include "keyboard.hpp"
#include "network.hpp"
#include <Arduino.h>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>

// Debugging statement for showing keyboard data
// #define SHOW_KEYBOARD_DATA

// last time the internal LED was turned on
uint32_t ledLastOn = 0;

void ShowUpdatedDeviceListInfo(void);
void OnRawPress(uint8_t);
void OnRawRelease(uint8_t);
void OnHIDExtrasPress(uint32_t, uint16_t);
void OnHIDExtrasRelease(uint32_t, uint16_t);
void ShowHIDExtrasPress(uint32_t, uint16_t);

void setup() {
  // configure the built in LED for output
  // we will use this as a status indicator for when a key is pressed and a
  // signal is being sent
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  while (!Serial && millis() < 4000) {
    // Wait for Serial
  } // wait for Arduino Serial Monitor
  Serial.println("\n\n\n");

  setupKeyboard();

  Serial.println();
  Serial.println("[Start]");
  Serial.println("Starting Ethernet with DHCP...");
  setupNetworking();

  digitalWrite(LED_BUILTIN, LOW);
}

uint16_t lastKey = 0;

void loop() {
  myusb.Task();
  ShowUpdatedDeviceListInfo();

  checkNetwork();

  processKeyboard();

  if (ledLastOn + 6 < millis()) {
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (networkStateChanged) {
    updateStatusLights(!!gotIP, !!tcp.connected());
  }
}
