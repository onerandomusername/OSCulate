// Simple USB Keyboard Forwarder
//
// This example is in the public domain

#include <Arduino.h>
#include <OSCMessage.h>
#include <QNEthernet.h>
#include <USBHost_t36.h>
#include <string>
#include <unordered_set>

using namespace std;
using namespace qindesign::network;

// You can have this one only output to the USB type Keyboard and
// not show the keyboard data on Serial...
#define SHOW_KEYBOARD_DATA

const char addressPrefix[] = "/keypress/";

u_int32_t ledLastOn = 0;

EthernetUDP udp = EthernetUDP(1);

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

unordered_set<string> unprocessedKeyDown;
unordered_set<string> unprocessedKeyUp;
unordered_set<string> keysDownOnHost;
bool state_changed = false;

#ifdef KEYBOARD_INTERFACE
uint8_t keyboard_last_leds = 0;
// #elif !defined(SHOW_KEYBOARD_DATA)
// #Warning: "USB type does not have Serial, so turning on SHOW_KEYBOARD_DATA"
// #define SHOW_KEYBOARD_DATA
#endif

bool gotIP = false;
IPAddress ip;
IPAddress DEST_IP = IPAddress(10, 101, 67, 117);
uint16_t outPort = 6379;

void ShowUpdatedDeviceListInfo(void);
void OnPress(int);
void OnRelease(int);
void OnRawPress(uint8_t);
void OnRawRelease(uint8_t);
void OnHIDExtrasPress(uint32_t, uint16_t);
void OnHIDExtrasRelease(uint32_t, uint16_t);
void ShowHIDExtrasPress(uint32_t, uint16_t);

void sendRemoteCommand(const char key[], bool isDown) {
  Serial.print("Got key: ");
  Serial.println(key);
  string address(addressPrefix);
  address += key;
  OSCMessage msg(address.c_str());
  msg.add(isDown ? 1.0 : 0.0);
  Serial.printf("Address: %s\r\n", address.c_str());
  udp.beginPacket(DEST_IP, outPort);
  msg.send(udp);   // send the bytes to the SLIP stream
  udp.endPacket(); // mark the end of the OSC Packet
  msg.empty();     // free space occupied by message
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
  Serial.println('\n\n\n');

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
  // Only needed to display...
  keyboard1.attachPress(OnPress);
  // keyboard1.attachRelease(OnRelease);
  keyboard1.attachRawPress(OnRawPress);
  keyboard1.attachRawRelease(OnRawRelease);
  // keyboard1.attachExtrasPress(OnHIDExtrasPress);
  // keyboard1.attachExtrasRelease(OnHIDExtrasRelease);

  Ethernet.onLinkState([](bool state) {
    if (state) {
      Serial.println("[Ethernet] Link ON");
    } else {
      Serial.println("[Ethernet] Link OFF");
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
    }
  });

  Serial.println();
  Serial.println("[Start]");
  Serial.println("Starting Ethernet with DHCP...");
  if (!Ethernet.begin()) {
    Serial.println("ERROR: Failed to start Ethernet");
    return;
  }

  Serial.printf("UDP set up to %u\r\n", outPort);

  digitalWrite(LED_BUILTIN, LOW);
}

uint16_t lastKey = 0;

void loop() {
  myusb.Task();
  ShowUpdatedDeviceListInfo();
  if (state_changed) {
    state_changed = false;
    digitalWrite(LED_BUILTIN, HIGH);
    ledLastOn = millis();
    if (!unprocessedKeyUp.empty()) {
      for (auto key : unprocessedKeyUp) {
        Serial.print("Need to send a key UP for: ");
        Serial.println(key.c_str());
        sendRemoteCommand(key.c_str(), false);
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

void OnHIDExtrasPress(uint32_t top, uint16_t key) {
#ifdef KEYBOARD_INTERFACE
  if (top == 0xc0000) {
    Keyboard.press(0XE400 | key);
#ifndef KEYMEDIA_INTERFACE
#error "KEYMEDIA_INTERFACE is Not defined"
#endif
  }
#endif
#ifdef SHOW_KEYBOARD_DATA
  ShowHIDExtrasPress(top, key);
#endif
}

void OnHIDExtrasRelease(uint32_t top, uint16_t key) {
#ifdef KEYBOARD_INTERFACE
  if (top == 0xc0000) {
    Keyboard.release(0XE400 | key);
  }
#endif
#ifdef SHOW_KEYBOARD_DATA
  Serial.print("HID (");
  Serial.print(top, HEX);
  Serial.print(") key release:");
  Serial.println(key, HEX);
#endif
}

string rawKeyToString(uint8_t keycode) {
  Serial.print("Keycode: ");
  Serial.println(keycode);

  uint16_t matcher = keycode | 0xF000;
  string key_equal;
  switch (matcher) {
  case KEY_A:
    key_equal = "a";
    break;
  case KEY_B:
    key_equal = "b";
    break;
  case KEY_C:
    key_equal = "c";
    break;
  case KEY_D:
    key_equal = "d";
    break;
  case KEY_E:
    key_equal = "e";
    break;
  case KEY_F:
    key_equal = "f";
    break;
  case KEY_G:
    key_equal = "g";
    break;
  case KEY_H:
    key_equal = "h";
    break;
  case KEY_I:
    key_equal = "i";
    break;
  case KEY_J:
    key_equal = "j";
    break;
  case KEY_K:
    key_equal = "k";
    break;
  case KEY_L:
    key_equal = "l";
    break;
  case KEY_M:
    key_equal = "m";
    break;
  case KEY_N:
    key_equal = "n";
    break;
  case KEY_O:
    key_equal = "o";
    break;
  case KEY_P:
    key_equal = "p";
    break;
  case KEY_Q:
    key_equal = "q";
    break;
  case KEY_R:
    key_equal = "r";
    break;
  case KEY_S:
    key_equal = "s";
    break;
  case KEY_T:
    key_equal = "t";
    break;
  case KEY_U:
    key_equal = "u";
    break;
  case KEY_V:
    key_equal = "v";
    break;
  case KEY_W:
    key_equal = "w";
    break;
  case KEY_X:
    key_equal = "x";
    break;
  case KEY_Y:
    key_equal = "y";
    break;
  case KEY_Z:
    key_equal = "z";
    break;
  case KEY_1:
    key_equal = "1";
    break;
  case KEY_2:
    key_equal = "2";
    break;
  case KEY_3:
    key_equal = "3";
    break;
  case KEY_4:
    key_equal = "4";
    break;
  case KEY_5:
    key_equal = "5";
    break;
  case KEY_6:
    key_equal = "6";
    break;
  case KEY_7:
    key_equal = "7";
    break;
  case KEY_8:
    key_equal = "8";
    break;
  case KEY_9:
    key_equal = "9";
    break;
  case KEY_0:
    key_equal = "0";
    break;
  case KEY_SPACE:
    key_equal = "Space";
    break;
  case KEY_ENTER:
    key_equal = "Enter";
    break;
  case KEY_TAB:
    key_equal = "Tab";
    break;
  case KEY_BACKSPACE:
    key_equal = "Backspace";
    break;
  case KEY_ESC:
    key_equal = "Escape";
    break;
  case KEY_UP:
    key_equal = "Up";
    break;
  case KEY_DOWN:
    key_equal = "Down";
    break;
  case KEY_LEFT:
    key_equal = "Left";
    break;
  case KEY_RIGHT:
    key_equal = "Right";
    break;
  case KEY_LEFT_GUI:
    key_equal = "LGUI";
    break;
  case KEY_RIGHT_GUI:
    key_equal = "RGUI";
    break;
  case KEY_LEFT_SHIFT:
    key_equal = "LShift";
    break;
  case KEY_RIGHT_SHIFT:
    key_equal = "RShift";
    break;
  case KEY_LEFT_CTRL:
    key_equal = "LCtrl";
    break;
  case KEY_RIGHT_CTRL:
    key_equal = "RCtrl";
    break;
  case KEY_LEFT_ALT:
    key_equal = "LAlt";
    break;
  case KEY_RIGHT_ALT:
    key_equal = "RAlt";
    break;
  case KEY_F1:
    key_equal = "F1";
    break;
  case KEY_F2:
    key_equal = "F2";
    break;
  case KEY_F3:
    key_equal = "F3";
    break;
  case KEY_F4:
    key_equal = "F4";
    break;
  case KEY_F5:
    key_equal = "F5";
    break;
  case KEY_F6:
    key_equal = "F6";
    break;
  case KEY_F7:
    key_equal = "F7";
    break;
  case KEY_F8:
    key_equal = "F8";
    break;
  case KEY_F9:
    key_equal = "F9";
    break;
  case KEY_F10:
    key_equal = "F10";
    break;
  case KEY_F11:
    key_equal = "F11";
    break;
  case KEY_F12:
    key_equal = "F12";
    break;
  default:
    key_equal = '\0';
    break;
  }
  Serial.print("Key Equal: ");
  Serial.println(key_equal.c_str());
  return key_equal;
}

void OnRawPress(uint8_t keycode) {
  unprocessedKeyDown.insert(rawKeyToString(keycode));
  state_changed = true;
#ifdef KEYBOARD_INTERFACE
  if (keyboard_leds != keyboard_last_leds) {
    Serial.printf("New LEDS: %x\n", keyboard_leds);
    keyboard_last_leds = keyboard_leds;
    keyboard1.LEDS(keyboard_leds);
  }
  if (keycode >= 103 && keycode < 111) {
    // one of the modifier keys was pressed, so lets turn it
    // on global..
    uint8_t keybit = 1 << (keycode - 103);
    keyboard_modifiers |= keybit;
    Keyboard.set_modifier(keyboard_modifiers);
  } else {
    if (keyboard1.getModifiers() != keyboard_modifiers) {
#ifdef SHOW_KEYBOARD_DATA
      Serial.printf("Mods mismatch: %x != %x\n", keyboard_modifiers,
                    keyboard1.getModifiers());
#endif
      keyboard_modifiers = keyboard1.getModifiers();
      Keyboard.set_modifier(keyboard_modifiers);
    }
    Keyboard.press(0XF000 | keycode);
  }
#endif
#ifdef SHOW_KEYBOARD_DATA
  Serial.print("OnRawPress keycode: ");
  Serial.print(keycode, HEX);
  Serial.print(" Modifiers: ");
  Serial.println(keyboard_modifiers, HEX);
#endif
}

void OnRawRelease(uint8_t keycode) {
  unprocessedKeyUp.insert(rawKeyToString(keycode));
  state_changed = true;
#ifdef KEYBOARD_INTERFACE
  if (keycode >= 103 && keycode < 111) {
    // one of the modifier keys was pressed, so lets turn it
    // on global..
    uint8_t keybit = 1 << (keycode - 103);
    keyboard_modifiers &= ~keybit;
    Keyboard.set_modifier(keyboard_modifiers);
  } else {
    Keyboard.release(0XF000 | keycode);
  }
#endif
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
#ifdef SHOW_KEYBOARD_DATA
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

#endif

void ShowUpdatedDeviceListInfo() {
#ifdef SHOW_KEYBOARD_DATA
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

#endif
}

void OnPress(int key) {
#ifdef SHOW_KEYBOARD_DATA
  string to_print;
  Serial.print("key '");
  switch (key) {
  case KEYD_UP:
    to_print = "UP";
    break;
  case KEYD_DOWN:
    to_print = "DN";
    break;
  case KEYD_LEFT:
    to_print = "LEFT";
    break;
  case KEYD_RIGHT:
    to_print = "RIGHT";
    break;
  case KEYD_INSERT:
    to_print = "Ins";
    break;
  case KEYD_DELETE:
    to_print = "Del";
    break;
  case KEYD_PAGE_UP:
    to_print = "PUP";
    break;
  case KEYD_PAGE_DOWN:
    to_print = "PDN";
    break;
  case KEYD_HOME:
    to_print = "HOME";
    break;
  case KEYD_END:
    to_print = "END";
    break;
  case KEYD_F1:
    to_print = "F1";
    break;
  case KEYD_F2:
    to_print = "F2";
    break;
  case KEYD_F3:
    to_print = "F3";
    break;
  case KEYD_F4:
    to_print = "F4";
    break;
  case KEYD_F5:
    to_print = "F5";
    break;
  case KEYD_F6:
    to_print = "F6";
    break;
  case KEYD_F7:
    to_print = "F7";
    break;
  case KEYD_F8:
    to_print = "F8";
    break;
  case KEYD_F9:
    to_print = "F9";
    break;
  case KEYD_F10:
    to_print = "F10";
    break;
  case KEYD_F11:
    to_print = "F11";
    break;
  case KEYD_F12:
    to_print = "F12";
    break;
  default:
    to_print = (char)key;
    break;
  }
  Serial.print(to_print.c_str());

  Serial.print("'  ");
  Serial.print(key);
  Serial.print(" MOD: ");
  Serial.print(keyboard1.getModifiers(), HEX);
  Serial.print(" OEM: ");
  Serial.print(keyboard1.getOemKey(), HEX);
  Serial.print(" LEDS: ");
  Serial.println(keyboard1.LEDS(), HEX);
  Serial.print("key ");
  Serial.print((char)keyboard1.getKey());
  // Serial.print("  ");
  // Serial.print((char)keyboard2.getKey());
  // Serial.println();
#endif

  string address(addressPrefix);
  address += (char)key;
  //   OSCMessage msg("/keypress/a");
  //   msg.add(1.0);
  Serial.printf("Address: %s\r\n", address.c_str());
  //   udp.beginPacket(DEST_IP, outPort);
  //   msg.send(udp); // send the bytes to the SLIP stream
  //   udp.endPacket(); // mark the end of the OSC Packet
  //   msg.empty(); // free space occupied by message
}

void OnRelease(int key) {
  string address(addressPrefix);
  address += (char)key;
  //   OSCMessage msg("keypress/a");
  //   msg.add(0);
  Serial.printf("Address: %s\r\n", address.c_str());
  //   udp.beginPacket(DEST_IP, outPort);
  //   msg.send(udp); // send the bytes to the SLIP stream
  //   udp.endPacket(); // mark the end of the OSC Packet
  //   msg.empty(); // free space occupied by message
}

void ShowHIDExtrasPress(uint32_t top, uint16_t key) {
#ifdef SHOW_KEYBOARD_DATA
  Serial.print("HID (");
  Serial.print(top, HEX);
  Serial.print(") key press:");
  Serial.print(key, HEX);
  if (top == 0xc0000) {
    switch (key) {
    case 0x20:
      Serial.print(" - +10");
      break;
    case 0x21:
      Serial.print(" - +100");
      break;
    case 0x22:
      Serial.print(" - AM/PM");
      break;
    case 0x30:
      Serial.print(" - Power");
      break;
    case 0x31:
      Serial.print(" - Reset");
      break;
    case 0x32:
      Serial.print(" - Sleep");
      break;
    case 0x33:
      Serial.print(" - Sleep After");
      break;
    case 0x34:
      Serial.print(" - Sleep Mode");
      break;
    case 0x35:
      Serial.print(" - Illumination");
      break;
    case 0x36:
      Serial.print(" - Function Buttons");
      break;
    case 0x40:
      Serial.print(" - Menu");
      break;
    case 0x41:
      Serial.print(" - Menu  Pick");
      break;
    case 0x42:
      Serial.print(" - Menu Up");
      break;
    case 0x43:
      Serial.print(" - Menu Down");
      break;
    case 0x44:
      Serial.print(" - Menu Left");
      break;
    case 0x45:
      Serial.print(" - Menu Right");
      break;
    case 0x46:
      Serial.print(" - Menu Escape");
      break;
    case 0x47:
      Serial.print(" - Menu Value Increase");
      break;
    case 0x48:
      Serial.print(" - Menu Value Decrease");
      break;
    case 0x60:
      Serial.print(" - Data On Screen");
      break;
    case 0x61:
      Serial.print(" - Closed Caption");
      break;
    case 0x62:
      Serial.print(" - Closed Caption Select");
      break;
    case 0x63:
      Serial.print(" - VCR/TV");
      break;
    case 0x64:
      Serial.print(" - Broadcast Mode");
      break;
    case 0x65:
      Serial.print(" - Snapshot");
      break;
    case 0x66:
      Serial.print(" - Still");
      break;
    case 0x80:
      Serial.print(" - Selection");
      break;
    case 0x81:
      Serial.print(" - Assign Selection");
      break;
    case 0x82:
      Serial.print(" - Mode Step");
      break;
    case 0x83:
      Serial.print(" - Recall Last");
      break;
    case 0x84:
      Serial.print(" - Enter Channel");
      break;
    case 0x85:
      Serial.print(" - Order Movie");
      break;
    case 0x86:
      Serial.print(" - Channel");
      break;
    case 0x87:
      Serial.print(" - Media Selection");
      break;
    case 0x88:
      Serial.print(" - Media Select Computer");
      break;
    case 0x89:
      Serial.print(" - Media Select TV");
      break;
    case 0x8A:
      Serial.print(" - Media Select WWW");
      break;
    case 0x8B:
      Serial.print(" - Media Select DVD");
      break;
    case 0x8C:
      Serial.print(" - Media Select Telephone");
      break;
    case 0x8D:
      Serial.print(" - Media Select Program Guide");
      break;
    case 0x8E:
      Serial.print(" - Media Select Video Phone");
      break;
    case 0x8F:
      Serial.print(" - Media Select Games");
      break;
    case 0x90:
      Serial.print(" - Media Select Messages");
      break;
    case 0x91:
      Serial.print(" - Media Select CD");
      break;
    case 0x92:
      Serial.print(" - Media Select VCR");
      break;
    case 0x93:
      Serial.print(" - Media Select Tuner");
      break;
    case 0x94:
      Serial.print(" - Quit");
      break;
    case 0x95:
      Serial.print(" - Help");
      break;
    case 0x96:
      Serial.print(" - Media Select Tape");
      break;
    case 0x97:
      Serial.print(" - Media Select Cable");
      break;
    case 0x98:
      Serial.print(" - Media Select Satellite");
      break;
    case 0x99:
      Serial.print(" - Media Select Security");
      break;
    case 0x9A:
      Serial.print(" - Media Select Home");
      break;
    case 0x9B:
      Serial.print(" - Media Select Call");
      break;
    case 0x9C:
      Serial.print(" - Channel Increment");
      break;
    case 0x9D:
      Serial.print(" - Channel Decrement");
      break;
    case 0x9E:
      Serial.print(" - Media Select SAP");
      break;
    case 0xA0:
      Serial.print(" - VCR Plus");
      break;
    case 0xA1:
      Serial.print(" - Once");
      break;
    case 0xA2:
      Serial.print(" - Daily");
      break;
    case 0xA3:
      Serial.print(" - Weekly");
      break;
    case 0xA4:
      Serial.print(" - Monthly");
      break;
    case 0xB0:
      Serial.print(" - Play");
      break;
    case 0xB1:
      Serial.print(" - Pause");
      break;
    case 0xB2:
      Serial.print(" - Record");
      break;
    case 0xB3:
      Serial.print(" - Fast Forward");
      break;
    case 0xB4:
      Serial.print(" - Rewind");
      break;
    case 0xB5:
      Serial.print(" - Scan Next Track");
      break;
    case 0xB6:
      Serial.print(" - Scan Previous Track");
      break;
    case 0xB7:
      Serial.print(" - Stop");
      break;
    case 0xB8:
      Serial.print(" - Eject");
      break;
    case 0xB9:
      Serial.print(" - Random Play");
      break;
    case 0xBA:
      Serial.print(" - Select DisC");
      break;
    case 0xBB:
      Serial.print(" - Enter Disc");
      break;
    case 0xBC:
      Serial.print(" - Repeat");
      break;
    case 0xBD:
      Serial.print(" - Tracking");
      break;
    case 0xBE:
      Serial.print(" - Track Normal");
      break;
    case 0xBF:
      Serial.print(" - Slow Tracking");
      break;
    case 0xC0:
      Serial.print(" - Frame Forward");
      break;
    case 0xC1:
      Serial.print(" - Frame Back");
      break;
    case 0xC2:
      Serial.print(" - Mark");
      break;
    case 0xC3:
      Serial.print(" - Clear Mark");
      break;
    case 0xC4:
      Serial.print(" - Repeat From Mark");
      break;
    case 0xC5:
      Serial.print(" - Return To Mark");
      break;
    case 0xC6:
      Serial.print(" - Search Mark Forward");
      break;
    case 0xC7:
      Serial.print(" - Search Mark Backwards");
      break;
    case 0xC8:
      Serial.print(" - Counter Reset");
      break;
    case 0xC9:
      Serial.print(" - Show Counter");
      break;
    case 0xCA:
      Serial.print(" - Tracking Increment");
      break;
    case 0xCB:
      Serial.print(" - Tracking Decrement");
      break;
    case 0xCD:
      Serial.print(" - Pause/Continue");
      break;
    case 0xE0:
      Serial.print(" - Volume");
      break;
    case 0xE1:
      Serial.print(" - Balance");
      break;
    case 0xE2:
      Serial.print(" - Mute");
      break;
    case 0xE3:
      Serial.print(" - Bass");
      break;
    case 0xE4:
      Serial.print(" - Treble");
      break;
    case 0xE5:
      Serial.print(" - Bass Boost");
      break;
    case 0xE6:
      Serial.print(" - Surround Mode");
      break;
    case 0xE7:
      Serial.print(" - Loudness");
      break;
    case 0xE8:
      Serial.print(" - MPX");
      break;
    case 0xE9:
      Serial.print(" - Volume Up");
      break;
    case 0xEA:
      Serial.print(" - Volume Down");
      break;
    case 0xF0:
      Serial.print(" - Speed Select");
      break;
    case 0xF1:
      Serial.print(" - Playback Speed");
      break;
    case 0xF2:
      Serial.print(" - Standard Play");
      break;
    case 0xF3:
      Serial.print(" - Long Play");
      break;
    case 0xF4:
      Serial.print(" - Extended Play");
      break;
    case 0xF5:
      Serial.print(" - Slow");
      break;
    case 0x100:
      Serial.print(" - Fan Enable");
      break;
    case 0x101:
      Serial.print(" - Fan Speed");
      break;
    case 0x102:
      Serial.print(" - Light");
      break;
    case 0x103:
      Serial.print(" - Light Illumination Level");
      break;
    case 0x104:
      Serial.print(" - Climate Control Enable");
      break;
    case 0x105:
      Serial.print(" - Room Temperature");
      break;
    case 0x106:
      Serial.print(" - Security Enable");
      break;
    case 0x107:
      Serial.print(" - Fire Alarm");
      break;
    case 0x108:
      Serial.print(" - Police Alarm");
      break;
    case 0x150:
      Serial.print(" - Balance Right");
      break;
    case 0x151:
      Serial.print(" - Balance Left");
      break;
    case 0x152:
      Serial.print(" - Bass Increment");
      break;
    case 0x153:
      Serial.print(" - Bass Decrement");
      break;
    case 0x154:
      Serial.print(" - Treble Increment");
      break;
    case 0x155:
      Serial.print(" - Treble Decrement");
      break;
    case 0x160:
      Serial.print(" - Speaker System");
      break;
    case 0x161:
      Serial.print(" - Channel Left");
      break;
    case 0x162:
      Serial.print(" - Channel Right");
      break;
    case 0x163:
      Serial.print(" - Channel Center");
      break;
    case 0x164:
      Serial.print(" - Channel Front");
      break;
    case 0x165:
      Serial.print(" - Channel Center Front");
      break;
    case 0x166:
      Serial.print(" - Channel Side");
      break;
    case 0x167:
      Serial.print(" - Channel Surround");
      break;
    case 0x168:
      Serial.print(" - Channel Low Frequency Enhancement");
      break;
    case 0x169:
      Serial.print(" - Channel Top");
      break;
    case 0x16A:
      Serial.print(" - Channel Unknown");
      break;
    case 0x170:
      Serial.print(" - Sub-channel");
      break;
    case 0x171:
      Serial.print(" - Sub-channel Increment");
      break;
    case 0x172:
      Serial.print(" - Sub-channel Decrement");
      break;
    case 0x173:
      Serial.print(" - Alternate Audio Increment");
      break;
    case 0x174:
      Serial.print(" - Alternate Audio Decrement");
      break;
    case 0x180:
      Serial.print(" - Application Launch Buttons");
      break;
    case 0x181:
      Serial.print(" - AL Launch Button Configuration Tool");
      break;
    case 0x182:
      Serial.print(" - AL Programmable Button Configuration");
      break;
    case 0x183:
      Serial.print(" - AL Consumer Control Configuration");
      break;
    case 0x184:
      Serial.print(" - AL Word Processor");
      break;
    case 0x185:
      Serial.print(" - AL Text Editor");
      break;
    case 0x186:
      Serial.print(" - AL Spreadsheet");
      break;
    case 0x187:
      Serial.print(" - AL Graphics Editor");
      break;
    case 0x188:
      Serial.print(" - AL Presentation App");
      break;
    case 0x189:
      Serial.print(" - AL Database App");
      break;
    case 0x18A:
      Serial.print(" - AL Email Reader");
      break;
    case 0x18B:
      Serial.print(" - AL Newsreader");
      break;
    case 0x18C:
      Serial.print(" - AL Voicemail");
      break;
    case 0x18D:
      Serial.print(" - AL Contacts/Address Book");
      break;
    case 0x18E:
      Serial.print(" - AL Calendar/Schedule");
      break;
    case 0x18F:
      Serial.print(" - AL Task/Project Manager");
      break;
    case 0x190:
      Serial.print(" - AL Log/Journal/Timecard");
      break;
    case 0x191:
      Serial.print(" - AL Checkbook/Finance");
      break;
    case 0x192:
      Serial.print(" - AL Calculator");
      break;
    case 0x193:
      Serial.print(" - AL A/V Capture/Playback");
      break;
    case 0x194:
      Serial.print(" - AL Local Machine Browser");
      break;
    case 0x195:
      Serial.print(" - AL LAN/WAN Browser");
      break;
    case 0x196:
      Serial.print(" - AL Internet Browser");
      break;
    case 0x197:
      Serial.print(" - AL Remote Networking/ISP Connect");
      break;
    case 0x198:
      Serial.print(" - AL Network Conference");
      break;
    case 0x199:
      Serial.print(" - AL Network Chat");
      break;
    case 0x19A:
      Serial.print(" - AL Telephony/Dialer");
      break;
    case 0x19B:
      Serial.print(" - AL Logon");
      break;
    case 0x19C:
      Serial.print(" - AL Logoff");
      break;
    case 0x19D:
      Serial.print(" - AL Logon/Logoff");
      break;
    case 0x19E:
      Serial.print(" - AL Terminal Lock/Screensaver");
      break;
    case 0x19F:
      Serial.print(" - AL Control Panel");
      break;
    case 0x1A0:
      Serial.print(" - AL Command Line Processor/Run");
      break;
    case 0x1A1:
      Serial.print(" - AL Process/Task Manager");
      break;
    case 0x1A2:
      Serial.print(" - AL Select Tast/Application");
      break;
    case 0x1A3:
      Serial.print(" - AL Next Task/Application");
      break;
    case 0x1A4:
      Serial.print(" - AL Previous Task/Application");
      break;
    case 0x1A5:
      Serial.print(" - AL Preemptive Halt Task/Application");
      break;
    case 0x200:
      Serial.print(" - Generic GUI Application Controls");
      break;
    case 0x201:
      Serial.print(" - AC New");
      break;
    case 0x202:
      Serial.print(" - AC Open");
      break;
    case 0x203:
      Serial.print(" - AC Close");
      break;
    case 0x204:
      Serial.print(" - AC Exit");
      break;
    case 0x205:
      Serial.print(" - AC Maximize");
      break;
    case 0x206:
      Serial.print(" - AC Minimize");
      break;
    case 0x207:
      Serial.print(" - AC Save");
      break;
    case 0x208:
      Serial.print(" - AC Print");
      break;
    case 0x209:
      Serial.print(" - AC Properties");
      break;
    case 0x21A:
      Serial.print(" - AC Undo");
      break;
    case 0x21B:
      Serial.print(" - AC Copy");
      break;
    case 0x21C:
      Serial.print(" - AC Cut");
      break;
    case 0x21D:
      Serial.print(" - AC Paste");
      break;
    case 0x21E:
      Serial.print(" - AC Select All");
      break;
    case 0x21F:
      Serial.print(" - AC Find");
      break;
    case 0x220:
      Serial.print(" - AC Find and Replace");
      break;
    case 0x221:
      Serial.print(" - AC Search");
      break;
    case 0x222:
      Serial.print(" - AC Go To");
      break;
    case 0x223:
      Serial.print(" - AC Home");
      break;
    case 0x224:
      Serial.print(" - AC Back");
      break;
    case 0x225:
      Serial.print(" - AC Forward");
      break;
    case 0x226:
      Serial.print(" - AC Stop");
      break;
    case 0x227:
      Serial.print(" - AC Refresh");
      break;
    case 0x228:
      Serial.print(" - AC Previous Link");
      break;
    case 0x229:
      Serial.print(" - AC Next Link");
      break;
    case 0x22A:
      Serial.print(" - AC Bookmarks");
      break;
    case 0x22B:
      Serial.print(" - AC History");
      break;
    case 0x22C:
      Serial.print(" - AC Subscriptions");
      break;
    case 0x22D:
      Serial.print(" - AC Zoom In");
      break;
    case 0x22E:
      Serial.print(" - AC Zoom Out");
      break;
    case 0x22F:
      Serial.print(" - AC Zoom");
      break;
    case 0x230:
      Serial.print(" - AC Full Screen View");
      break;
    case 0x231:
      Serial.print(" - AC Normal View");
      break;
    case 0x232:
      Serial.print(" - AC View Toggle");
      break;
    case 0x233:
      Serial.print(" - AC Scroll Up");
      break;
    case 0x234:
      Serial.print(" - AC Scroll Down");
      break;
    case 0x235:
      Serial.print(" - AC Scroll");
      break;
    case 0x236:
      Serial.print(" - AC Pan Left");
      break;
    case 0x237:
      Serial.print(" - AC Pan Right");
      break;
    case 0x238:
      Serial.print(" - AC Pan");
      break;
    case 0x239:
      Serial.print(" - AC New Window");
      break;
    case 0x23A:
      Serial.print(" - AC Tile Horizontally");
      break;
    case 0x23B:
      Serial.print(" - AC Tile Vertically");
      break;
    case 0x23C:
      Serial.print(" - AC Format");
      break;
    }
  }
  Serial.println();
#endif
}
