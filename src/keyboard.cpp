
#include "config.h"
#include "osc_base.h"
#include "ulog.h"
#include <Arduino.h>
#include <OSCMessage.h>
#include <USBHost_t36.h>
#include <string>
#include <unordered_set>

// USB Host
USBHost myusb;
USBHub hub1(myusb);
KeyboardController keyboard1(myusb);

USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);

// used to force an update of the keyboard LEDs every second
// the Usb HID spec does not involve the keyboard sending us its state
// so we need to keep a local spec
// the keyboard is not always ready when plugged in, and takes an indeterminate
// amount of time to be ready. so we need to keep trying to set the LEDs
// until it works.
// Future enchancement would be not sending constantly, but only during the
// first 10 seconds of connection
elapsedMillis ledsLastUpdated;

// modifier keys, stored as a bitfield
// from bits 1<<0 to 1<<7:
// 0 (Left Control)
// 1 (Left Shift)
// 2 (Left Alt)
// 3 (Left GUI)
// 4 (Right Control)
// 5 (Right Shift)
// 6 (Right Alt)
// 7 (Right GUI)
uint8_t keyboard_modifiers = 0; // try to keep a reasonable value

// unprocessed key presses
// we cannot call a network function from the interrupt when a key is pressed in
// our callbacks. so we need to store the key presses and releases in a set and
// then process them in the main loop.
std::unordered_set<std::string> unprocessedKeyDown;
std::unordered_set<uint16_t> unprocessedKeyUp;
// if there's anything in unprocessedKeyUp or unprocessedKeyDown, we need to
// send it.
bool state_changed = false;
/// @brief Map of key codes to OSC commands that are currently pressed.
/// This is used to map key codes to OSC commands. The key codes
/// are the USB HID key codes. The OSC commands are the OSC commands that will
/// be sent to the remote device when the key is pressed or released.
/// these are the key codes that are currently pressed.
std::unordered_map<std::uint16_t, std::string> keyToCommand;

/// @brief Convert a keypress into the OSC Key that Eos expects.
/// @param keycode the raw keycode that was pressed on a keyboard.
/// @return The OSC key that corresponds to the keypress.
std::string rawKeytoOSCCommand(uint8_t keycode) {
  ULOG_INFO("Keyboard Modifiers: 0x%02X", keyboard_modifiers);
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
    ULOG_INFO("Key Equal: %s", key_equal.c_str());
  }
  return key_equal;
}

/// @brief Return the name of the key that was pressed
/// @param keycode the raw keycode that was pressed on a keyboard.
/// @return the name of the pressed key
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

  ULOG_INFO("Key Equal: %s", key_equal.c_str());
  return key_equal;
}

/// @brief Handle a raw key press event from the keyboard.
/// @param keycode the raw keycode that was pressed on a keyboard.
void OnRawPress(uint8_t keycode) {
  const std::string keypressed = rawKeytoOSCCommand(keycode);
  ULOG_INFO(!keypressed.empty() ? "Key Pressed" : "Key is empty");
  if (!keypressed.empty()) {
    keyToCommand[keycode] = keypressed;
    unprocessedKeyDown.insert(keypressed);
    state_changed = true;
  } else {
    ULOG_INFO("Key not found in map ; but why do we error??");
    ULOG_INFO("Odd keycode? %u", keycode);
  }
  if (keycode >= 103 && keycode < 111) {
    // one of the modifier keys was pressed, so lets turn it
    // on global..
    keyboard_modifiers |= 1 << (keycode - 103);
  }
#ifdef SHOW_KEYBOARD_DATA
  ULOG_INFO("OnRawPress keycode: 0x%02X", keycode);
  ULOG_INFO(" Modifiers: 0x%02X", keyboard_modifiers);
#endif
}

/// @brief  Handle a raw key release event from the keyboard.
/// @param keycode the raw keycode that was released on a keyboard.
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
  ULOG_INFO("OnRawRelease keycode: 0x%02X", keycode);
  ULOG_INFO(" Modifiers: 0x%02X", keyboard1.getModifiers());
#endif
}

//=============================================================
// Device and Keyboard Output To Serial objects...
//=============================================================
USBDriver *drivers[] = {&hub1, &hid1, &hid2, &hid3};
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = {"Hub1", "HID1", "HID2", "HID3"};
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
        ULOG_INFO("*** Device %s - disconnected ***", driver_names[i]);
        driver_active[i] = false;
      } else {
        ULOG_INFO("*** Device %s %x:%x - connected ***", driver_names[i],
                  drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz)
          ULOG_INFO("  manufacturer: %s", psz);
        psz = drivers[i]->product();
        if (psz && *psz)
          ULOG_INFO("  product: %s", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz)
          ULOG_INFO("  Serial: %s", psz);
      }
    }
  }
  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        ULOG_INFO("*** HID Device %s - disconnected ***", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        ULOG_INFO("*** HID Device %s %x:%x - connected ***",
                  hid_driver_names[i], hiddrivers[i]->idVendor(),
                  hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz)
          ULOG_INFO("  manufacturer: %s", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz)
          ULOG_INFO("  product: %s", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz)
          ULOG_INFO("  Serial: %s", psz);
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
        ULOG_INFO("*** BTHID Device %s - disconnected ***",
                  bthid_driver_names[i]);
        bthid_driver_active[i] = false;
      } else {
        ULOG_INFO("*** BTHID Device %s %x:%x - connected ***",
                  bthid_driver_names[i], bthiddrivers[i]->idVendor(),
                  bthiddrivers[i]->idProduct());
        bthid_driver_active[i] = true;
        const uint8_t *psz = bthiddrivers[i]->manufacturer();
        if (psz && *psz)
          ULOG_INFO("  manufacturer: %s", psz);
        psz = bthiddrivers[i]->product();
        if (psz && *psz)
          ULOG_INFO("  product: %s", psz);
        psz = bthiddrivers[i]->serialNumber();
        if (psz && *psz)
          ULOG_INFO("  Serial: %s", psz);
        if (bthiddrivers[i] == &keyboard1) {
          // try force back to HID mode
          ULOG_INFO("\n Try to force keyboard back into HID protocol");
          keyboard1.forceHIDProtocol();
        }
      }
    }
  }
}

void ShowHIDExtrasPress(uint32_t top, uint16_t key) {
  if (top != 0xc0000) {
    return;
  };

  std::string key_equal =
      (keymap_key_to_name.find(key) != keymap_key_to_name.end())
          ? keymap_key_to_name.at(key)
          : "\0";

  ULOG_INFO("HID (0x%02X) key press: 0x%02X %s", top, key, key_equal.c_str());
}

void setupKeyboard() {
#ifdef SHOW_KEYBOARD_DATA

  ULOG_INFO("\n\nUSB Host Keyboard forward and Testing");
  ULOG_INFO(sizeof(USBHub), DEC);
#endif
  myusb.begin();
  ULOG_INFO("USB Host started");
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
};

void processKeyboard(OSCClient &client) {

  if (!unprocessedKeyUp.empty()) {
    for (auto key : unprocessedKeyUp) {
      ULOG_INFO("Need to send a key UP for: %u", key);
      if (keyToCommand.find(key) != keyToCommand.end()) {
        auto command = keyToCommand.at(key);
        keyToCommand.erase(key);
        ULOG_INFO(command.c_str());
        client.sendEosKey(command.c_str(), false);
      } else {
        ULOG_INFO("Key not down, can't up ");
      }
    }
    unprocessedKeyUp.clear();
  }
  if (!unprocessedKeyDown.empty()) {
    for (auto key : unprocessedKeyDown) {
      ULOG_INFO("Need to send a key DOWN for: %s", key.c_str());
      client.sendEosKey(key.c_str(), true);
    }
    unprocessedKeyDown.clear();
  }
};

void updateStatusLights(bool hasIP, bool connectedToConsole) {
  KeyboardController::KBDLeds_t ledState = {keyboard1.LEDS()};
  ledState.numLock = hasIP;
  ledState.scrollLock = connectedToConsole;
  if (ledState.byte != keyboard1.LEDS()) {
    keyboard1.LEDS(ledState.byte);
    ledsLastUpdated = 0;
  } else if (ledsLastUpdated > 1000) {
    keyboard1.updateLEDS();
    ledsLastUpdated = 0;
  }
}
