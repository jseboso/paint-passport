#include "cloud.h"
#include "config.h"
#include "media.h"
#include "wifi_setup.h"
#include "cloud_secrets.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SD.h>

static const uint32_t INBOX_POLL_INTERVAL_MS = 30000;
// Primed so the first check fires soon after boot instead of waiting a full
// interval - unsigned wraparound makes millis() - lastInboxPollMs come out
// large on the very first call.
static uint32_t lastInboxPollMs = 0u - INBOX_POLL_INTERVAL_MS;

void cloudInit() {
  Serial0.print("Cloud: device=");
  Serial0.println(CLOUD_DEVICE_ID);
}

// Pulls a top-level "key":"value" string field out of a small, known-shape
// JSON response. Not a real JSON parser - fine for the fixed-shape responses
// this backend returns, and keeps us from needing a JSON library dependency
// on top of what the ESP32 board package already ships.
static String extractJsonString(const String& body, const char* key) {
  String needle = String("\"") + key + "\":\"";
  int start = body.indexOf(needle);
  if (start < 0) return String();
  start += needle.length();
  int end = body.indexOf('"', start);
  if (end < 0) return String();
  String value = body.substring(start, end);
  value.replace("\\/", "/");
  return value;
}

// ─────────────────────────────────────────────
//  Device -> S3 -> web gallery
// ─────────────────────────────────────────────
bool cloudUploadPainting(const char* localPath) {
  if (!wifiIsConnected()) return false;

  File f = SD.open(localPath);
  if (!f) {
    Serial0.println("Cloud upload: local painting file missing");
    return false;
  }
  size_t fileLen = f.size();

  // 1) ask the backend for a presigned S3 upload URL
  WiFiClientSecure presignClient;
  presignClient.setInsecure();
  HTTPClient presignHttp;
  presignHttp.setTimeout(10000);

  char presignUrl[160];
  snprintf(presignUrl, sizeof(presignUrl), "%s/devices/%s/paintings/presign", CLOUD_API_BASE, CLOUD_DEVICE_ID);
  presignHttp.begin(presignClient, presignUrl);
  presignHttp.addHeader("x-device-key", CLOUD_DEVICE_KEY);

  int presignCode = presignHttp.POST("");
  if (presignCode != 200) {
    Serial0.printf("Cloud upload: presign failed (HTTP %d)\n", presignCode);
    presignHttp.end();
    f.close();
    return false;
  }
  String uploadUrl = extractJsonString(presignHttp.getString(), "uploadUrl");
  presignHttp.end();

  if (uploadUrl.length() == 0) {
    Serial0.println("Cloud upload: couldn't parse presign response");
    f.close();
    return false;
  }

  // 2) stream the BMP straight from SD to S3 - no need to buffer it in RAM
  WiFiClientSecure uploadClient;
  uploadClient.setInsecure();
  HTTPClient uploadHttp;
  uploadHttp.setTimeout(20000);
  uploadHttp.begin(uploadClient, uploadUrl);
  uploadHttp.addHeader("Content-Type", "image/bmp");

  int putCode = uploadHttp.sendRequest("PUT", &f, fileLen);
  uploadHttp.end();
  f.close();

  Serial0.printf("Cloud upload: PUT status %d\n", putCode);
  return putCode == 200;
}

// ─────────────────────────────────────────────
//  Web -> S3 -> device inbox
// ─────────────────────────────────────────────
static bool downloadInboxImage(const String& url) {
  char sdPath[24];
  if (!beginInboxReceive(sdPath, sizeof(sdPath))) {
    Serial0.println("Cloud inbox: gallery full or SD unavailable, skipping");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(20000);
  http.begin(client, url);

  int code = http.GET();
  if (code != 200) {
    Serial0.printf("Cloud inbox: download failed (HTTP %d)\n", code);
    http.end();
    return false;
  }

  File out = SD.open(sdPath, FILE_WRITE);
  if (!out) {
    Serial0.println("Cloud inbox: couldn't open SD file for writing");
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  int total = http.getSize();
  uint8_t buf[512];
  int written = 0;
  uint32_t lastProgressMs = millis();

  while (http.connected() && (total < 0 || written < total)) {
    size_t avail = stream->available();
    if (avail > 0) {
      int n = stream->readBytes(buf, (size_t)min((int)avail, (int)sizeof(buf)));
      out.write(buf, n);
      written += n;
      lastProgressMs = millis();
    } else if (millis() - lastProgressMs > 15000) {
      Serial0.println("Cloud inbox: download stalled, giving up");
      break;
    } else {
      delay(2);
    }
  }
  out.close();
  http.end();

  if (total >= 0 && written != total) {
    Serial0.printf("Cloud inbox: incomplete download (%d/%d bytes)\n", written, total);
    return false;
  }

  Serial0.printf("Cloud inbox: downloaded %d bytes\n", written);
  return finishInboxReceive();
}

static void acknowledgeInbox() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);

  char url[160];
  snprintf(url, sizeof(url), "%s/devices/%s/inbox/ack", CLOUD_API_BASE, CLOUD_DEVICE_ID);
  http.begin(client, url);
  http.addHeader("x-device-key", CLOUD_DEVICE_KEY);

  int code = http.POST("");
  Serial0.printf("Cloud inbox: ack status %d\n", code);
  http.end();
}

void cloudTick() {
  if (!wifiIsConnected()) return;
  // don't interrupt anything the user is actively doing
  if (appMode != MODE_PAINT || activePanel != PANEL_NONE || drawing) return;
  if (!sdAvailable()) return;
  if (millis() - lastInboxPollMs < INBOX_POLL_INTERVAL_MS) return;
  lastInboxPollMs = millis();

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);

  char url[160];
  snprintf(url, sizeof(url), "%s/devices/%s/inbox", CLOUD_API_BASE, CLOUD_DEVICE_ID);
  http.begin(client, url);
  http.addHeader("x-device-key", CLOUD_DEVICE_KEY);

  int code = http.GET();
  if (code != 200) {
    if (code > 0) Serial0.printf("Cloud inbox poll: HTTP %d\n", code);
    http.end();
    return;
  }
  String body = http.getString();
  http.end();

  if (body.indexOf("\"available\":true") < 0) return;  // nothing waiting

  String downloadUrl = extractJsonString(body, "url");
  if (downloadUrl.length() == 0) return;

  Serial0.println("Cloud inbox: new image found, downloading...");
  if (downloadInboxImage(downloadUrl)) {
    acknowledgeInbox();
  }
}
