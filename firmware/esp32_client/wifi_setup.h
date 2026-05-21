#pragma once
#include "config.h"
#include <stddef.h>

//  WiFi connectivity + full-screen setup UI
// tap a network, type its password on the on-screen keyboard, connect.
// Working credentials get saved to flash and auto-reconnect on boot.

static const uint8_t MAX_WIFI_NETWORKS = 10;

void wifiInit();  // call once from setup() - loads saved creds, starts auto-connect
void wifiTick();  // call every loop() - advances scan/connect state, animates the screen

void openWifiScreen();

void handleWifiTouch(bool isTouching, int16_t x, int16_t y);

bool wifiIsConnected();
const char* wifiStatusLine(char* out, size_t outLen);  // one-liner for the Settings panel

// defined in esp32_client.ino
void drawCloseButton(uint16_t x, uint16_t y);
void drawToolbar();
void drawSettingsPanel();
