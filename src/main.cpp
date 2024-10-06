// Simple USB Keyboard Forwarder
//
// This example is in the public domain

#include "SLIPEncodedTCP.h"
#include "config.h"
#include <Arduino.h>
#include <OSCMessage.h>
#include <QNEthernet.h>
#include <USBHost_t36.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace qindesign::network;

// You can have this one only output to the USB type Keyboard and
// not show the keyboard data on Serial...
// #define SHOW_KEYBOARD_DATA

const char addressPrefix[] = "/eos/key/";

u_int32_t ledLastOn = 0;

// TCPConnection
EthernetClient tcp = EthernetClient();
SLIPEncodedTCP slip(tcp);

elapsedMillis sinceLastConnectAttempt;

USBHost myusb;
USBHub hub1(myusb);
KeyboardController keyboard1(myusb);
// BluetoothController bluet(myusb, true, "0000");   // Version does pairing to
// device
BluetoothController bluet(myusb); // version assumes it already was paired

USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);

uint8_t keyboard_modifiers = 0; // try to keep a reasonable value

std::unordered_set<std::string> unprocessedKeyDown;
std::unordered_set<uint16_t> unprocessedKeyUp;
/// @brief Map of key codes to OSC commands that are currently pressed.
/// @details This map is used to map key codes to OSC commands. The key codes
/// are the USB HID key codes. The OSC commands are the OSC commands that will
/// be sent to the remote device when the key is pressed or released.
/// these are the key codes that are currently pressed.
std::unordered_map<std::uint16_t, std::string> keyToCommand;
bool state_changed = false;

#ifdef KEYBOARD_INTERFACE
uint8_t keyboard_last_leds = 0;
// #elif !defined(SHOW_KEYBOARD_DATA)
// #Warning: "USB type does not have Serial, so turning on SHOW_KEYBOARD_DATA"
// #define SHOW_KEYBOARD_DATA
#endif

bool gotIP = false;
IPAddress ip;

void ShowUpdatedDeviceListInfo(void);
void OnRawPress(uint8_t);
void OnRawRelease(uint8_t);
void OnHIDExtrasPress(uint32_t, uint16_t);
void OnHIDExtrasRelease(uint32_t, uint16_t);
void ShowHIDExtrasPress(uint32_t, uint16_t);

void sendOSCviaPacketLength(OSCMessage &msg) {
  uint8_t buffer[4];
  const auto len = msg.bytes();
  // make the first four bytes the count of len
  buffer[0] = (len >> 24) & 0xFF;
  buffer[1] = (len >> 16) & 0xFF;
  buffer[2] = (len >> 8) & 0xFF;
  buffer[3] = len & 0xFF;
  tcp.write(buffer, 4);
  msg.send(tcp);
  tcp.flush();
}

void sendOSCviaSLIP(OSCMessage &msg) {
  slip.beginPacket();
  msg.send(slip);
  slip.endPacket();
}

void sendRemoteCommand(const char key[], bool isDown) {
  Serial.print("Got key: ");
  Serial.println(key);
  std::string address(addressPrefix);
  address += key;
  OSCMessage msg(address.c_str());
  msg.add(isDown ? 1.0 : 0.0);
  Serial.printf("Address: %s\r\n", address.c_str());

  if (oscversion == OSCVersion::SLIP) {
    sendOSCviaSLIP(msg);
  } else {
    sendOSCviaPacketLength(msg);
  }

  msg.empty(); // free space occupied by message
}

bool getEthernetIPFromNetwork() {
  if (!!Ethernet.localIP() && Ethernet.linkState()) {
    return false;
  }
  if (!Ethernet.begin()) {
    Serial.println("ERROR: Failed to start Ethernet");
    return false;
  }

  if (!Ethernet.waitForLink(fallbackWaitTime)) {
    Serial.println("Ethernet link is not up");
    return false;
  }

  if (!Ethernet.waitForLocalIP(fallbackWaitTime)) {
    Serial.println("Failed to get IP address, trying static");
    Ethernet.begin(staticIP, staticSubnetMask, INADDR_NONE);
  }

  Serial.println("Ethernet started");
  return !!Ethernet.localIP();
}

bool connectToLXConsole() {
  if (!tcp.connectionId()) {
    if (!tcp.connect(DEST_IP, outPort)) {
      return false;
    }
    tcp.setConnectionTimeout(600);
    Serial.println("Connected to LX console.");
  }
  return true;
}

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

#ifdef SHOW_KEYBOARD_DATA

  Serial.println("\n\nUSB Host Keyboard forward and Testing");
  Serial.println(sizeof(USBHub), DEC);
#endif
  myusb.begin();
  Serial.println("USB Host started");
#ifdef KEYBOARD_INTERFACE
  Keyboard.begin();
  Keyboard.press(KEY_NUM_LOCK);
  delay(600);
  Keyboard.release(KEY_NUM_LOCK);
#endif
  keyboard1.attachRawPress(OnRawPress);
  keyboard1.attachRawRelease(OnRawRelease);
  // keyboard1.attachExtrasPress(OnHIDExtrasPress);
  // keyboard1.attachExtrasRelease(OnHIDExtrasRelease);

  Ethernet.onLinkState([](bool state) {
    if (state) {
      Serial.println("[Ethernet] Link ON");
    } else {
      Serial.println("[Ethernet] Link OFF");
      gotIP = false;
      if (tcp) {
        tcp.abort();
        Serial.println("[Ethernet] Aborted TCP Connection");
      }
    }
  });

  // Watch for address changes
  // It will take a little time to get an IP address, so watch for it
  Ethernet.onAddressChanged([]() {
    IPAddress ip = Ethernet.localIP();
    bool hasIP = (ip != INADDR_NONE);
    if (hasIP) {
      gotIP = true;
      Serial.printf("[Ethernet] Address changed:\r\n");
      ip = Ethernet.localIP();
      Serial.printf("    Local IP     = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
      Serial.println(Ethernet.isDHCPActive() ? "    DHCP" : "    Static IP");
      ip = Ethernet.subnetMask();
      Serial.printf("    Subnet mask  = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
      ip = Ethernet.broadcastIP();
      Serial.printf("    Broadcast IP = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
      ip = Ethernet.gatewayIP();
      Serial.printf("    Gateway      = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
      ip = Ethernet.dnsServerIP();
      Serial.printf("    DNS          = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
    } else {
      Serial.println("[Ethernet] Address changed: No IP");
      gotIP = false;
    }
  });

  Serial.println();
  Serial.println("[Start]");
  Serial.println("Starting Ethernet with DHCP...");
  Ethernet.setHostname(HOSTNAME);

  getEthernetIPFromNetwork();
  connectToLXConsole();

  digitalWrite(LED_BUILTIN, LOW);
}

uint16_t lastKey = 0;

void loop() {
  myusb.Task();
  ShowUpdatedDeviceListInfo();

  if (!gotIP) {
    getEthernetIPFromNetwork();
  }
  if (!tcp.connectionId()) {
    if (sinceLastConnectAttempt > TCPConnectionCheckTime) {
      sinceLastConnectAttempt = 0;
      if (!connectToLXConsole()) {
        Serial.println("Failed to connect to LX Console");
      }
    }
  }

  if (tcp.available()) {
    while (tcp.available()) {
      tcp.read();
    }
  }

  if (state_changed) {
    state_changed = false;
    digitalWrite(LED_BUILTIN, HIGH);
    ledLastOn = millis();
    Serial.println("State changed, sending commands");
    if (!unprocessedKeyUp.empty()) {
      for (auto key : unprocessedKeyUp) {
        Serial.print("Need to send a key UP for: ");
        Serial.println(key);
        if (keyToCommand.find(key) != keyToCommand.end()) {
          auto command = keyToCommand.at(key);
          keyToCommand.erase(key);
          Serial.println(command.c_str());
          sendRemoteCommand(command.c_str(), false);
        } else {
          Serial.println("Key not down, can't up ");
        }
      }
      unprocessedKeyUp.clear();
    }
    if (!unprocessedKeyDown.empty()) {
      for (auto key : unprocessedKeyDown) {
        Serial.print("Need to send a key DOWN for: ");
        Serial.println(key.c_str());
        sendRemoteCommand(key.c_str(), true);
      }
      unprocessedKeyDown.clear();
    }
  }
  if (ledLastOn + 6 < millis()) {
    digitalWrite(LED_BUILTIN, LOW);
  }
}

std::string rawKeytoOSCCommand(uint8_t keycode) {
  Serial.println(keyboard_modifiers, HEX);
  const bool CONTROL_PRESSED = keyboard_modifiers & 0b00010001;
  const bool SHIFT_PRESSED = keyboard_modifiers & 0b00100010;
  const bool ALT_PRESSED = keyboard_modifiers & 0b01000100;

  uint16_t matcher;
  if (keycode >= 103 && keycode < 111) {
    uint8_t keybit = 1 << (keycode - 103);
    matcher = keybit | 0xE000;
  } else
    matcher = keycode | 0xF000;

  if (CONTROL_PRESSED || SHIFT_PRESSED || ALT_PRESSED) {
    matcher |= CONTROL_PRESSED << 9;
    matcher |= SHIFT_PRESSED << 10;
    matcher |= ALT_PRESSED << 11;
  }
  std::string key_equal;
  if (!(KeyCombosToCommands.find(matcher) == KeyCombosToCommands.end())) {
    key_equal = KeyCombosToCommands.at(matcher);
    //  try again but with no modifiers
  } else if (!(KeyCombosToCommands.find(keycode | 0xF000) ==
               KeyCombosToCommands.end())) {
    key_equal = KeyCombosToCommands.at(keycode | 0xF000);

  } else {
    key_equal = "";
  }
  if (!key_equal.empty()) {
    Serial.print("Key Equal: ");
    Serial.println(key_equal.c_str());
  }
  return key_equal;
}

std::string rawKeyToStringPassThrough(uint8_t keycode) {
  uint16_t matcher;
  if (keycode >= 103 && keycode < 111) {
    uint8_t keybit = 1 << (keycode - 103);
    matcher = keybit | 0xE000;
  } else
    matcher = keycode | 0xF000;

  std::string key_equal =
      (keymap_key_to_name.find(matcher) != keymap_key_to_name.end())
          ? keymap_key_to_name.at(matcher)
          : "\0";

  Serial.print("Key Equal: ");
  Serial.println(key_equal.c_str());
  return key_equal;
}

void OnRawPress(uint8_t keycode) {
  const std::string keypressed = rawKeytoOSCCommand(keycode);
  Serial.println(!keypressed.empty() ? "Key Pressed" : "Key is empty");
  if (!keypressed.empty()) {
    keyToCommand[keycode] = keypressed;
    unprocessedKeyDown.insert(keypressed);
    state_changed = true;
  } else {
    Serial.println("Key not found in map ; but why do we error??");
    Serial.print("Odd keycode? ");
    Serial.println(keycode);
  }
  if (keycode >= 103 && keycode < 111) {
    // one of the modifier keys was pressed, so lets turn it
    // on global..
    keyboard_modifiers |= 1 << (keycode - 103);
  }
#ifdef SHOW_KEYBOARD_DATA
  Serial.print("OnRawPress keycode: ");
  Serial.print(keycode, HEX);
  Serial.print(" Modifiers: ");
  Serial.println(keyboard_modifiers, HEX);
#endif
}

void OnRawRelease(uint8_t keycode) {
  if (keycode >= 103 && keycode < 111) {
    // one of the modifier keys was pressed, so lets turn it
    // on global..
    keyboard_modifiers &= ~(1 << (keycode - 103));
  } else {
    unprocessedKeyUp.insert(keycode);
    state_changed = true;
  }
#ifdef SHOW_KEYBOARD_DATA
  Serial.print("OnRawRelease keycode: ");
  Serial.print(keycode, HEX);
  Serial.print(" Modifiers: ");
  Serial.println(keyboard1.getModifiers(), HEX);
#endif
}

//=============================================================
// Device and Keyboard Output To Serial objects...
//=============================================================
USBDriver *drivers[] = {&hub1, &hid1, &hid2, &hid3, &bluet};
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = {"Hub1", "HID1", "HID2", "HID3",
                                         "BlueTooth"};
bool driver_active[CNT_DEVICES] = {false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&keyboard1};
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char *hid_driver_names[CNT_DEVICES] = {"KB"};
bool hid_driver_active[CNT_DEVICES] = {false};

BTHIDInput *bthiddrivers[] = {&keyboard1};
#define CNT_BTHIDDEVICES (sizeof(bthiddrivers) / sizeof(bthiddrivers[0]))
const char *bthid_driver_names[CNT_HIDDEVICES] = {"KB(BT)"};
bool bthid_driver_active[CNT_HIDDEVICES] = {false};

void ShowUpdatedDeviceListInfo() {
  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i],
                      drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz)
          Serial.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz)
          Serial.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz)
          Serial.printf("  Serial: %s\n", psz);
      }
    }
  }
  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n",
                      hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial.printf("*** HID Device %s %x:%x - connected ***\n",
                      hid_driver_names[i], hiddrivers[i]->idVendor(),
                      hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz)
          Serial.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz)
          Serial.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz)
          Serial.printf("  Serial: %s\n", psz);
        // Note: with some keyboards there is an issue that they may not
        // output in a format understand either as in boot format or in a
        // HID format that is recognized.  In that case you can try
        // forcing the keyboard into boot mode.
        if (hiddrivers[i] == &keyboard1) {
          // example Gigabyte uses N key rollover which should now
          // work, but...
        }
      }
    }
  }
  for (uint8_t i = 0; i < CNT_BTHIDDEVICES; i++) {
    if (*bthiddrivers[i] != bthid_driver_active[i]) {
      if (bthid_driver_active[i]) {
        Serial.printf("*** BTHID Device %s - disconnected ***\n",
                      bthid_driver_names[i]);
        bthid_driver_active[i] = false;
      } else {
        Serial.printf("*** BTHID Device %s %x:%x - connected ***\n",
                      bthid_driver_names[i], bthiddrivers[i]->idVendor(),
                      bthiddrivers[i]->idProduct());
        bthid_driver_active[i] = true;
        const uint8_t *psz = bthiddrivers[i]->manufacturer();
        if (psz && *psz)
          Serial.printf("  manufacturer: %s\n", psz);
        psz = bthiddrivers[i]->product();
        if (psz && *psz)
          Serial.printf("  product: %s\n", psz);
        psz = bthiddrivers[i]->serialNumber();
        if (psz && *psz)
          Serial.printf("  Serial: %s\n", psz);
        if (bthiddrivers[i] == &keyboard1) {
          // try force back to HID mode
          Serial.println("\n Try to force keyboard back into HID protocol");
          keyboard1.forceHIDProtocol();
        }
      }
    }
  }
}

void ShowHIDExtrasPress(uint32_t top, uint16_t key) {
  Serial.print("HID (");
  Serial.print(top, HEX);
  Serial.print(") key press:");
  Serial.print(key, HEX);

  if (top != 0xc0000) {
    return;
  };

  std::string key_equal =
      (keymap_key_to_name.find(key) != keymap_key_to_name.end())
          ? keymap_key_to_name.at(key)
          : "\0";

  Serial.println(key_equal.c_str());
}
