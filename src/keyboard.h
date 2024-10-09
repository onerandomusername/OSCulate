#pragma once

#ifndef keyboard_h
#define keyboard_h

#include "osc_base.h"
#include <USBHost_t36.h>

extern USBHost myusb;

extern bool state_changed;

void setupKeyboard();
void processKeyboard(OSCClient &client);
void updateStatusLights(bool hasIP, bool connectedToConsole);
void ShowUpdatedDeviceListInfo();

#endif // keyboard_h
