

#include "config.h"
#include "keyboard.h"
#include "network.h"
#include "osc_base.h"
#include "ulog.h"
#include <Arduino.h>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>

void my_console_logger(ulog_level_t severity, char *msg) {
  Serial.printf("[%s]: %s\n", ulog_level_name(severity), msg);
}

// Debugging statement for showing keyboard data
// #define SHOW_KEYBOARD_DATA

// last time the internal LED was turned on
uint32_t ledLastOn = 0;

void setup() {
  // configure the built in LED for output
  // we will use this as a status indicator for when a key is pressed and a
  // signal is being sent
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  ULOG_INIT();
  ULOG_SUBSCRIBE(my_console_logger, ULOG_DEBUG_LEVEL);

  while (!Serial && millis() < 4000) {
    // Wait for Serial
  } // wait for Arduino Serial Monitor
  ULOG_INFO("\n\n\n");

  ULOG_INFO("Logging configured."); // logs to file and console

  setupKeyboard();

  ULOG_INFO("[Start]");
  ULOG_INFO("Starting Ethernet with DHCP...");
  setupNetworking();

  digitalWrite(LED_BUILTIN, LOW);
}

uint16_t lastKey = 0;

void loop() {
  myusb.Task();
  ShowUpdatedDeviceListInfo();

  checkNetwork();

  if (state_changed) {
    state_changed = false;
    digitalWrite(LED_BUILTIN, HIGH);
    ledLastOn = millis();
    ULOG_DEBUG("State changed, sending commands");
    processKeyboard(client);
  };

  if (ledLastOn + 6 < millis()) {
    digitalWrite(LED_BUILTIN, LOW);
  }

  updateStatusLights(!!gotIP, !!client.isConnected());
}
