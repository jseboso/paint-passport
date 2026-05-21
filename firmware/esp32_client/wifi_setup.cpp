#include "wifi_setup.h"
#include "canvas.h"
#include "keyboard.h"
#include <WiFi.h>
#include <Preferences.h>
#include <string.h>

//  Layout
static const uint16_t TITLE_H  = 64;
static const uint16_t CLOSE_X  = SCREEN_W - CLOSE_BTN_SIZE - 12;
static const uint16_t CLOSE_Y  = 10;
static const uint16_t PAD      = 14;
static const uint16_t ROW_H    = 64;
static const uint16_t LIST_TOP = TITLE_H + 96;
static const uint16_t RESCAN_Y = TITLE_H + 44;
static const uint16_t FIELD_Y  = TITLE_H + 40;
static const uint16_t FIELD_H  = 56;
static const uint16_t CONNECT_Y = FIELD_Y + FIELD_H + 14;

static Preferences prefs;
static char savedSSID[33] = "";
static bool hasSaved      = false;

enum WifiScreen { WSCR_LIST, WSCR_PASSWORD, WSCR_CONNECTING, WSCR_RESULT };
static WifiScreen wscreen = WSCR_LIST;

struct WifiNetwork { char ssid[33]; int32_t rssi; bool secure; };
static WifiNetwork networks[MAX_WIFI_NETWORKS];
static uint8_t     networkCount = 0;
static bool        scanning     = false;

static char selectedSSID[33] = "";
static bool selectedSecure   = false;
static char passwordBuf[65];
static Keyboard kb;

static uint32_t connectStartMs = 0;
static char     resultMessage[64] = "";
static bool     resultSuccess     = false;

//  Touch registering
static bool wifiWasTouching  = false;
static bool wifiArmed        = false;
static bool wifiClosePending = false;

//  Forward declarations
static void drawTitleBar(const char* title);
static void drawListScreen();
static void drawPasswordScreenChrome();
static void drawConnectingScreen();
static void drawResultScreen();
static void startScan();
static void enterPasswordScreen(const WifiNetwork& net);
static void reenterPasswordScreen();
static void attemptConnect();
static void closeWifiScreenNow();

void wifiInit() {
  WiFi.mode(WIFI_STA);
  prefs.begin("wifi", false);
  size_t n = prefs.getString("ssid", savedSSID, sizeof(savedSSID));
  hasSaved = (n > 0);
  if (hasSaved) {
    char pass[65] = "";
    prefs.getString("pass", pass, sizeof(pass));
    Serial0.print("WiFi: auto-connecting to ");
    Serial0.println(savedSSID);
    WiFi.begin(savedSSID, pass);  // non-blocking
  } else {
    Serial0.println("WiFi: no saved network");
  }
}

bool wifiIsConnected() { return WiFi.status() == WL_CONNECTED; }

const char* wifiStatusLine(char* out, size_t outLen) {
  if (wifiIsConnected()) {
    snprintf(out, outLen, "Connected: %s", savedSSID[0] ? savedSSID : "?");
  } else if (hasSaved) {
    snprintf(out, outLen, "Not connected (saved: %s)", savedSSID);
  } else {
    snprintf(out, outLen, "Not connected");
  }
  return out;
}

//  Scanning
static void startScan() {
  WiFi.scanDelete();
  networkCount = 0;
  scanning = true;
  WiFi.scanNetworks(true /*async*/, false /*show_hidden*/);
  drawListScreen();
}

static void collectScanResults(int n) {
  networkCount = 0;
  for (int i = 0; i < n && networkCount < MAX_WIFI_NETWORKS; i++) {
    // scan accessors are String-only on this core; copy into fixed buffers
    // immediately so the rest of this file doesn't need to touch String
    String ssidS = WiFi.SSID(i);
    if (ssidS.length() == 0) continue;

    int32_t rssi  = WiFi.RSSI(i);
    bool    secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;

    bool dup = false;
    for (uint8_t j = 0; j < networkCount; j++) {
      if (strncmp(networks[j].ssid, ssidS.c_str(), sizeof(networks[j].ssid)) == 0) {
        dup = true;
        if (rssi > networks[j].rssi) networks[j].rssi = rssi;
        break;
      }
    }
    if (dup) continue;

    strncpy(networks[networkCount].ssid, ssidS.c_str(), sizeof(networks[networkCount].ssid) - 1);
    networks[networkCount].ssid[sizeof(networks[networkCount].ssid) - 1] = '\0';
    networks[networkCount].rssi   = rssi;
    networks[networkCount].secure = secure;
    networkCount++;
  }

  // Strongest signal first
  for (uint8_t i = 1; i < networkCount; i++) {
    WifiNetwork key = networks[i];
    int8_t j = (int8_t)(i - 1);
    while (j >= 0 && networks[j].rssi < key.rssi) {
      networks[j + 1] = networks[j];
      j--;
    }
    networks[j + 1] = key;
  }

  WiFi.scanDelete();
}

//  Connecting
static void attemptConnect() {
  WiFi.disconnect();
  if (selectedSecure) {
    WiFi.begin(selectedSSID, passwordBuf);
  } else {
    WiFi.begin(selectedSSID);
  }
  connectStartMs = millis();
  wscreen = WSCR_CONNECTING;
  drawConnectingScreen();
}

// Screen transitions
void openWifiScreen() {
  if (!undoSpriteReady) {
    // no scratch buffer to snapshot the canvas into, so no way back
    Serial0.println("WiFi screen unavailable: no PSRAM sprite");
    return;
  }

  uint16_t rowBuf[CANVAS_W];
  for (int16_t row = 0; row < CANVAS_H; row++) {
    display.readRect(0, row, CANVAS_W, 1, rowBuf);
    undoSprite.pushImage(0, row, CANVAS_W, 1, rowBuf);
  }

  appMode          = MODE_WIFI;
  wscreen          = WSCR_LIST;
  wifiArmed        = false;
  wifiWasTouching  = false;
  wifiClosePending = false;
  startScan();
}

static void closeWifiScreenNow() {
  appMode = MODE_PAINT;
  undoSprite.pushSprite(0, 0);  // restore canvas
  drawToolbar();
  drawSettingsPanel();
  panelArmed = false;   // the touch that closed this screen shouldn't also hit Settings content
}

static void enterPasswordScreen(const WifiNetwork& net) {
  strncpy(selectedSSID, net.ssid, sizeof(selectedSSID));
  selectedSSID[sizeof(selectedSSID) - 1] = '\0';
  selectedSecure = net.secure;
  reenterPasswordScreen();
}

static void reenterPasswordScreen() {
  wscreen = WSCR_PASSWORD;
  uint16_t kbTop = SCREEN_H - KB_H;
  kbOpen(kb, passwordBuf, sizeof(passwordBuf), kbTop, true /*masked*/);
  drawPasswordScreenChrome();
}

//  Drawing
static void drawTitleBar(const char* title) {
  display.fillRect(0, 0, SCREEN_W, TITLE_H, COL_TOOLBAR_BG);
  display.drawFastHLine(0, TITLE_H, SCREEN_W, COL_MIDGREY);
  display.setTextColor(COL_WHITE, COL_TOOLBAR_BG);
  display.setTextSize(3);
  display.setCursor(PAD, 16);
  display.print(title);
  drawCloseButton(CLOSE_X, CLOSE_Y);
}

static void drawListScreen() {
  display.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_DARKGREY);
  drawTitleBar("Wi-Fi Setup");

  display.setTextColor(COL_MIDGREY, COL_DARKGREY);
  display.setTextSize(2);
  display.setCursor(PAD, TITLE_H + 12);
  char line[64];
  if (hasSaved) {
    snprintf(line, sizeof(line), wifiIsConnected() ? "Connected: %s" : "Saved: %s (not connected)", savedSSID);
  } else {
    snprintf(line, sizeof(line), "No network saved");
  }
  display.print(line);

  display.fillRoundRect(SCREEN_W - 140 - PAD, RESCAN_Y, 140, 40, 6, COL_BTN_ACTIVE);
  display.setTextColor(COL_WHITE, COL_BTN_ACTIVE);
  display.setTextSize(2);
  display.setCursor(SCREEN_W - 140 - PAD + 18, RESCAN_Y + 10);
  display.print("RESCAN");

  display.fillRect(0, LIST_TOP, SCREEN_W, SCREEN_H - LIST_TOP, COL_DARKGREY);

  if (scanning) {
    display.setTextColor(COL_WHITE, COL_DARKGREY);
    display.setTextSize(2);
    display.setCursor(PAD, LIST_TOP + 20);
    display.print("Scanning...");
    return;
  }

  if (networkCount == 0) {
    display.setTextColor(COL_MIDGREY, COL_DARKGREY);
    display.setTextSize(2);
    display.setCursor(PAD, LIST_TOP + 20);
    display.print("No networks found");
    return;
  }

  for (uint8_t i = 0; i < networkCount; i++) {
    uint16_t ry = (uint16_t)(LIST_TOP + i * ROW_H);
    if (ry + ROW_H > SCREEN_H) break;

    display.drawFastHLine(0, ry, SCREEN_W, COL_MIDGREY);
    display.setTextColor(COL_WHITE, COL_DARKGREY);
    display.setTextSize(2);
    display.setCursor(PAD, ry + 20);
    display.print(networks[i].ssid);

    uint8_t bars = networks[i].rssi > -55 ? 4 : networks[i].rssi > -65 ? 3 : networks[i].rssi > -75 ? 2 : 1;
    for (uint8_t b = 0; b < 4; b++) {
      uint16_t bh = (uint16_t)(8 + b * 6);
      uint32_t c  = b < bars ? COL_WHITE : COL_MIDGREY;
      display.fillRect(SCREEN_W - 100 + b * 12, ry + ROW_H / 2 + 14 - bh, 8, bh, c);
    }
    if (networks[i].secure) {
      display.drawRect(SCREEN_W - 140, ry + ROW_H / 2 - 8, 14, 12, COL_WHITE);
      display.fillRect(SCREEN_W - 137, ry + ROW_H / 2 - 14, 8, 8, COL_WHITE);
    }
  }
}

static void drawPasswordScreenChrome() {
  uint16_t kbTop = SCREEN_H - KB_H;
  display.fillRect(0, 0, SCREEN_W, kbTop - KB_GAP, COL_DARKGREY);  // everything above the keyboard
  drawTitleBar(selectedSSID);

  display.setTextColor(COL_MIDGREY, COL_DARKGREY);
  display.setTextSize(2);
  display.setCursor(PAD, TITLE_H + 8);
  display.print("Enter password");

  kbDrawField(kb, PAD, FIELD_Y, SCREEN_W - 2 * PAD, FIELD_H, "Password");

  display.fillRoundRect(PAD, CONNECT_Y, SCREEN_W - 2 * PAD, 48, 8, COL_BTN_ACTIVE);
  display.setTextColor(COL_WHITE, COL_BTN_ACTIVE);
  display.setTextSize(2);
  display.setCursor(SCREEN_W / 2 - 46, CONNECT_Y + 14);
  display.print("CONNECT");
}

static void drawConnectingScreen() {
  display.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_DARKGREY);
  drawTitleBar("Connecting");

  static uint8_t dots = 0;
  dots = (uint8_t)((dots + 1) % 4);
  char dotStr[4] = "";
  for (uint8_t i = 0; i < dots; i++) dotStr[i] = '.';
  dotStr[dots] = '\0';

  char line[48];
  snprintf(line, sizeof(line), "Connecting to %s%s", selectedSSID, dotStr);

  display.fillRect(0, SCREEN_H / 2 - 24, SCREEN_W, 30, COL_DARKGREY);
  display.setTextColor(COL_WHITE, COL_DARKGREY);
  display.setTextSize(2);
  display.setCursor(PAD, SCREEN_H / 2 - 20);
  display.print(line);
}

static void drawResultScreen() {
  display.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_DARKGREY);
  drawTitleBar(resultSuccess ? "Connected" : "Connection Failed");

  display.setTextColor(resultSuccess ? COL_WHITE : rgb(220, 80, 80), COL_DARKGREY);
  display.setTextSize(2);
  display.setCursor(PAD, SCREEN_H / 2 - 40);
  display.print(resultMessage);

  uint16_t btnY = SCREEN_H / 2 + 20;
  if (resultSuccess) {
    display.fillRoundRect(PAD, btnY, SCREEN_W - 2 * PAD, 48, 8, COL_BTN_ACTIVE);
    display.setTextColor(COL_WHITE, COL_BTN_ACTIVE);
    display.setCursor(SCREEN_W / 2 - 30, btnY + 14);
    display.print("DONE");
  } else {
    uint16_t halfW = (uint16_t)((SCREEN_W - 3 * PAD) / 2);
    display.fillRoundRect(PAD, btnY, halfW, 48, 8, COL_BTN_ACTIVE);
    display.setTextColor(COL_WHITE, COL_BTN_ACTIVE);
    display.setCursor(PAD + halfW / 2 - 30, btnY + 14);
    display.print("RETRY");

    uint16_t backX = (uint16_t)(PAD + halfW + PAD);
    display.fillRoundRect(backX, btnY, halfW, 48, 8, COL_MIDGREY);
    display.setTextColor(COL_WHITE, COL_MIDGREY);
    display.setCursor(backX + halfW / 2 - 28, btnY + 14);
    display.print("BACK");
  }
}

//  Per-frame progress (scan completion, connect timeout/result)
void wifiTick() {
  if (appMode == MODE_WIFI && wscreen == WSCR_LIST && scanning) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      scanning = false;
      collectScanResults(n);
      drawListScreen();
    } else if (n == WIFI_SCAN_FAILED) {
      scanning = false;
      networkCount = 0;
      drawListScreen();
    }
    // n == WIFI_SCAN_RUNNING: still in progress, nothing to do yet
    return;
  }

  // keeps polling even if the user backed out mid-connect
  if (wscreen == WSCR_CONNECTING) {
    bool visible = (appMode == MODE_WIFI);
    wl_status_t st = WiFi.status();
    uint32_t elapsed = millis() - connectStartMs;

    if (st == WL_CONNECTED) {
      resultSuccess = true;
      IPAddress ip = WiFi.localIP();
      snprintf(resultMessage, sizeof(resultMessage), "Connected -- %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      prefs.putString("ssid", selectedSSID);
      prefs.putString("pass", passwordBuf);
      strncpy(savedSSID, selectedSSID, sizeof(savedSSID));
      savedSSID[sizeof(savedSSID) - 1] = '\0';
      hasSaved = true;
      wscreen = WSCR_RESULT;
      if (visible) drawResultScreen();
    } else if (elapsed > 15000) {
      resultSuccess = false;
      snprintf(resultMessage, sizeof(resultMessage), "Couldn't connect (timed out)");
      wscreen = WSCR_RESULT;
      if (visible) drawResultScreen();
    } else if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
      resultSuccess = false;
      snprintf(resultMessage, sizeof(resultMessage),
               st == WL_NO_SSID_AVAIL ? "Network not found" : "Wrong password or connection refused");
      wscreen = WSCR_RESULT;
      if (visible) drawResultScreen();
    } else if (visible) {
      static uint32_t lastAnim = 0;
      if (millis() - lastAnim > 500) {
        lastAnim = millis();
        drawConnectingScreen();
      }
    }
  }
}

//  Touch handling
void handleWifiTouch(bool isTouching, int16_t x, int16_t y) {
  if (!isTouching) {
    wifiWasTouching = false;
    wifiArmed = true;
    if (wifiClosePending) {
      wifiClosePending = false;
      closeWifiScreenNow();
    }
    return;
  }

  // same idea as panelArmed: swallow touches until a lift has happened
  // since this screen opened, or since Close was tapped
  if (!wifiArmed || wifiClosePending) return;
  if (wifiWasTouching) return;  // everything here is tap-to-act, not drag
  wifiWasTouching = true;

  if (x >= CLOSE_X && x <= CLOSE_X + CLOSE_BTN_SIZE && y >= CLOSE_Y && y <= CLOSE_Y + CLOSE_BTN_SIZE) {
    wifiClosePending = true;
    return;
  }

  switch (wscreen) {
    case WSCR_LIST: {
      if (x >= SCREEN_W - 140 - PAD && x <= SCREEN_W - PAD && y >= RESCAN_Y && y <= RESCAN_Y + 40) {
        startScan();
        return;
      }
      if (!scanning && y >= LIST_TOP) {
        uint8_t idx = (uint8_t)((y - LIST_TOP) / ROW_H);
        if (idx < networkCount) {
          const WifiNetwork& net = networks[idx];
          if (net.secure) {
            enterPasswordScreen(net);
          } else {
            strncpy(selectedSSID, net.ssid, sizeof(selectedSSID));
            selectedSSID[sizeof(selectedSSID) - 1] = '\0';
            selectedSecure = false;
            passwordBuf[0] = '\0';
            attemptConnect();
          }
        }
      }
      return;
    }

    case WSCR_PASSWORD: {
      if (x >= PAD && x <= SCREEN_W - PAD && y >= CONNECT_Y && y <= CONNECT_Y + 48) {
        attemptConnect();
        return;
      }
      KbResult r = kbHandleTouch(kb, x, y);
      if (r == KB_EDITED) {
        kbDrawField(kb, PAD, FIELD_Y, SCREEN_W - 2 * PAD, FIELD_H, "Password");
      } else if (r == KB_DONE) {
        attemptConnect();
      }
      return;
    }

    case WSCR_CONNECTING:
      return;  // only the close button is interactive here

    case WSCR_RESULT: {
      uint16_t btnY = SCREEN_H / 2 + 20;
      if (y < btnY || y > btnY + 48) return;

      if (resultSuccess) {
        if (x >= PAD && x <= SCREEN_W - PAD) wifiClosePending = true;
        return;
      }

      uint16_t halfW = (uint16_t)((SCREEN_W - 3 * PAD) / 2);
      if (x >= PAD && x <= PAD + halfW) {
        // RETRY
        if (selectedSecure) {
          reenterPasswordScreen();
        } else {
          wscreen = WSCR_LIST;
          drawListScreen();
        }
      } else if (x >= PAD + halfW + PAD) {
        wscreen = WSCR_LIST;
        drawListScreen();
      }
      return;
    }
  }
}
