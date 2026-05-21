#pragma once
#include "config.h"
#include <stddef.h>

//  WiFi connectivity + full-screen setup UI

static const uint8_t MAX_WIFI_NETWORKS = 10;

void wifiInit();

void wifiTick();

void openWifiScreen();

void handleWifiTouch(bool isTouching, int16_t x, int16_t y);

bool wifiIsConnected();

const char* wifiStatusLine(char* out, size_t outLen);

// Defined in esp32_client.ino
void drawCloseButton(uint16_t x, uint16_t y);
void drawToolbar();
void drawSettingsPanel();
