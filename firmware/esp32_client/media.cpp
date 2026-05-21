#include "media.h"
#include "canvas.h"

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
static void cacheThumbnail(uint8_t index);
static void drawThumbnail(uint8_t index, uint16_t screenX);

// ─────────────────────────────────────────────
//  SD state
// ─────────────────────────────────────────────
static bool  _sdReady          = false;
uint8_t      paintingCount     = 0;
int16_t      galleryScrollX    = 0;
bool         mediaChromeDrawn  = false;
static char  paintingNames[MAX_PAINTINGS][24];

// ─────────────────────────────────────────────
//  Thumbnail cache (PSRAM)
// ─────────────────────────────────────────────
static uint16_t* thumbCache[MAX_PAINTINGS]  = {nullptr};
static bool      thumbCached[MAX_PAINTINGS] = {false};

static void cacheThumbnail(uint8_t index) {
  if (thumbCached[index]) return;

  thumbCache[index] = (uint16_t*)ps_malloc(THUMB_W * THUMB_H * 2);
  if (!thumbCache[index]) {
    Serial0.print("PSRAM alloc failed for thumb "); Serial0.println(index);
    return;
  }

  char thumbPath[28];
  snprintf(thumbPath, sizeof(thumbPath), "/thumbnails/%03d.bmp", index);
  File t = SD.open(thumbPath);
  if (!t) {
    free(thumbCache[index]);
    thumbCache[index] = nullptr;
    return;
  }

  t.seek(54);
  for (uint16_t row = 0; row < THUMB_H; row++) {
    uint16_t storeRow = THUMB_H - 1 - row;  // BMP is bottom-up
    for (uint16_t col = 0; col < THUMB_W; col++) {
      uint8_t b = t.read();
      uint8_t g = t.read();
      uint8_t r = t.read();
      thumbCache[index][storeRow * THUMB_W + col] = display.color565(r, g, b);
    }
    uint16_t pad = (4 - (THUMB_W * 3) % 4) % 4;
    for (uint8_t p = 0; p < pad; p++) t.read();
  }
  t.close();
  thumbCached[index] = true;
}

static void drawThumbnail(uint8_t index, uint16_t screenX) {
  cacheThumbnail(index);
  if (thumbCached[index] && thumbCache[index]) {
    display.pushImage(screenX, THUMB_Y, THUMB_W, THUMB_H, thumbCache[index]);
  } else {
    display.fillRect(screenX, THUMB_Y, THUMB_W, THUMB_H, COL_MIDGREY);
  }
}

// ─────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────
bool initSD() {
  Serial0.println("Starting SD (SPI)...");
  SPI.begin(12, 13, 11, 10);  // SCK, MISO, MOSI, CS
  Serial0.println("SPI started");
  delay(500);
  _sdReady = SD.begin(10, SPI, 4000000);
  Serial0.println(_sdReady ? "SD OK" : "SD failed");
  if (_sdReady) {
    if (!SD.exists("/paintings"))  SD.mkdir("/paintings");
    if (!SD.exists("/thumbnails")) SD.mkdir("/thumbnails");
    scanPaintings();
    for (uint8_t i = 0; i < paintingCount; i++) {
      cacheThumbnail(i);
    }
    Serial0.print("Card size: ");
    Serial0.print(SD.cardSize() / (1024 * 1024));
    Serial0.println("MB");
  }
  return _sdReady;
}

bool sdAvailable() { return _sdReady; }

// ─────────────────────────────────────────────
//  Scan existing paintings
// ─────────────────────────────────────────────
void scanPaintings() {
  paintingCount = 0;
  File dir = SD.open("/paintings");
  if (!dir) return;
  while (paintingCount < MAX_PAINTINGS) {
    File f = dir.openNextFile();
    if (!f) break;
    if (!f.isDirectory()) {
      snprintf(paintingNames[paintingCount],
               sizeof(paintingNames[0]),
               "/paintings/%s", f.name());
      paintingCount++;
    }
    f.close();
  }
  dir.close();
}

// ─────────────────────────────────────────────
//  BMP helpers
// ─────────────────────────────────────────────
static void writeU16(File& f, uint16_t v) {
  f.write(v & 0xFF);
  f.write((v >> 8) & 0xFF);
}

static void writeU32(File& f, uint32_t v) {
  f.write(v & 0xFF);
  f.write((v >> 8) & 0xFF);
  f.write((v >> 16) & 0xFF);
  f.write((v >> 24) & 0xFF);
}

static void writeBMPHeader(File& f, uint16_t w, uint16_t h) {
  uint32_t rowSize   = ((w * 3 + 3) / 4) * 4;
  uint32_t pixelData = 54;
  uint32_t fileSize  = pixelData + rowSize * h;
  f.write('B'); f.write('M');
  writeU32(f, fileSize);
  writeU32(f, 0);
  writeU32(f, pixelData);
  writeU32(f, 40);
  writeU32(f, w);
  writeU32(f, h);
  writeU16(f, 1);
  writeU16(f, 24);
  writeU32(f, 0);
  writeU32(f, rowSize * h);
  writeU32(f, 2835);
  writeU32(f, 2835);
  writeU32(f, 0);
  writeU32(f, 0);
}

// ─────────────────────────────────────────────
//  Save painting
// ─────────────────────────────────────────────
bool savePainting() {
  if (!_sdReady) return false;
  if (paintingCount >= MAX_PAINTINGS) return false;  // gallery full

  uint8_t idx = paintingCount;
  char fullPath[24], thumbPath[28];
  snprintf(fullPath, sizeof(fullPath), "/paintings/%03d.bmp", idx);
  snprintf(thumbPath, sizeof(thumbPath), "/thumbnails/%03d.bmp", idx);

  // Full canvas
  File f = SD.open(fullPath, FILE_WRITE);
  if (!f) return false;
  writeBMPHeader(f, CANVAS_W, CANVAS_H);
  uint16_t buf[CANVAS_W];
  for (int16_t row = CANVAS_H - 1; row >= 0; row--) {
    display.readRect(0, row, CANVAS_W, 1, buf);
    for (uint16_t col = 0; col < CANVAS_W; col++) {
      uint16_t px = buf[col];
      uint8_t r = ((px >> 11) & 0x1F) << 3;
      uint8_t g = ((px >> 5)  & 0x3F) << 2;
      uint8_t b = ( px        & 0x1F) << 3;
      f.write(b); f.write(g); f.write(r);
    }
    uint16_t pad = (4 - (CANVAS_W * 3) % 4) % 4;
    for (uint8_t p = 0; p < pad; p++) f.write((uint8_t)0);
  }
  f.close();

  // Thumbnail
  File t = SD.open(thumbPath, FILE_WRITE);
  if (t) {
    writeBMPHeader(t, THUMB_W, THUMB_H);
    for (int16_t row = THUMB_H - 1; row >= 0; row--) {
      uint16_t srcY = (uint32_t)row * CANVAS_H / THUMB_H;
      display.readRect(0, srcY, CANVAS_W, 1, buf);
      for (uint16_t col = 0; col < THUMB_W; col++) {
        uint16_t srcX = (uint32_t)col * CANVAS_W / THUMB_W;
        uint16_t px = buf[srcX];
        uint8_t r = ((px >> 11) & 0x1F) << 3;
        uint8_t g = ((px >> 5)  & 0x3F) << 2;
        uint8_t b = ( px        & 0x1F) << 3;
        t.write(b); t.write(g); t.write(r);
      }
      uint16_t pad = (4 - (THUMB_W * 3) % 4) % 4;
      for (uint8_t p = 0; p < pad; p++) t.write((uint8_t)0);
    }
    t.close();
  }

  snprintf(paintingNames[paintingCount], sizeof(paintingNames[0]), "%s", fullPath);
  paintingCount++;

  // Cache new thumbnail into PSRAM
  thumbCached[idx] = false;
  cacheThumbnail(idx);

  return true;
}

// ─────────────────────────────────────────────
//  Load painting
// ─────────────────────────────────────────────
void loadPainting(uint8_t index) {
  if (!_sdReady || index >= paintingCount) return;

  File f = SD.open(paintingNames[index]);
  if (!f) return;

  f.seek(54);
  uint16_t buf[CANVAS_W];
  for (int16_t row = CANVAS_H - 1; row >= 0; row--) {
    for (uint16_t col = 0; col < CANVAS_W; col++) {
      uint8_t b = f.read();
      uint8_t g = f.read();
      uint8_t r = f.read();
      buf[col] = display.color565(r, g, b);
    }
    uint16_t pad = (4 - (CANVAS_W * 3) % 4) % 4;
    for (uint8_t p = 0; p < pad; p++) f.read();
    display.pushImage(0, row, CANVAS_W, 1, buf);
  }
  f.close();

  // loaded image becomes the new undo baseline: no strokes on top of it
  nextStrokeIndex = 0;
  strokeCount = 0;
  snprintf(loadedBaselinePath, sizeof(loadedBaselinePath), "%s", paintingNames[index]);
}

// Same BMP layout as loadPainting(), but writes into a sprite instead of the
// live display. Used by canvas.cpp to rebuild the undo baseline without a
// visible flash on the screen.
bool loadBMPToSprite(const char* path, LGFX_Sprite& sprite) {
  if (!_sdReady) return false;

  File f = SD.open(path);
  if (!f) return false;

  f.seek(54);
  uint16_t buf[CANVAS_W];
  for (int16_t row = CANVAS_H - 1; row >= 0; row--) {
    for (uint16_t col = 0; col < CANVAS_W; col++) {
      uint8_t b = f.read();
      uint8_t g = f.read();
      uint8_t r = f.read();
      buf[col] = display.color565(r, g, b);
    }
    uint16_t pad = (4 - (CANVAS_W * 3) % 4) % 4;
    for (uint8_t p = 0; p < pad; p++) f.read();
    sprite.pushImage(0, row, CANVAS_W, 1, buf);
  }
  f.close();
  return true;
}

// ─────────────────────────────────────────────
//  Draw media panel
// ─────────────────────────────────────────────
void drawMediaPanel() {
  if (!mediaChromeDrawn) {
    drawPanelBase("Media");

    // Save button
    uint32_t saveBg = _sdReady ? rgb(40, 140, 40) : COL_MIDGREY;
    display.fillRoundRect(PANEL_CONTENT_X + PANEL_CONTENT_W - 90,
                          PANEL_CONTENT_Y - PANEL_TITLE_H + 4,
                          88, 28, 6, saveBg);
    display.setTextColor(COL_WHITE, saveBg);
    display.setTextSize(2);
    display.setCursor(PANEL_CONTENT_X + PANEL_CONTENT_W - 84,
                      PANEL_CONTENT_Y - PANEL_TITLE_H + 11);
    display.print(_sdReady ? "SAVE" : "NO SD");

    mediaChromeDrawn = true;
  }

  // Only redraw thumbnail strip
  display.fillRect(0, THUMB_Y, SCREEN_W, THUMB_H + 4, COL_DARKGREY);

  if (paintingCount == 0) {
    display.setTextColor(COL_MIDGREY, COL_DARKGREY);
    display.setTextSize(2);
    display.setCursor(PANEL_CONTENT_X, THUMB_Y + THUMB_H / 2 - 8);
    display.print("No saved paintings");
  } else {
    for (uint8_t i = 0; i < paintingCount; i++) {
      int16_t tx = PANEL_CONTENT_X + i * (THUMB_W + THUMB_PAD) - galleryScrollX;
      if (tx + THUMB_W <= 0 || tx >= SCREEN_W) continue;
      if (tx >= 0 && tx + THUMB_W <= SCREEN_W) {
        drawThumbnail(i, tx);
        display.drawRect(tx, THUMB_Y, THUMB_W, THUMB_H, COL_WHITE);
      } else {
        int16_t visX = max((int16_t)0, tx);
        int16_t visW = min((int16_t)SCREEN_W, (int16_t)(tx + THUMB_W)) - visX;
        display.fillRect(visX, THUMB_Y, visW, THUMB_H, COL_MIDGREY);
      }
    }
  }
}

// ─────────────────────────────────────────────
//  Touch handling
// ─────────────────────────────────────────────
static int16_t mediaDragStartX = -1;
static int16_t mediaScrollStart = 0;
static bool    mediaDidDrag     = false;

void handleMediaTouch(int16_t x, int16_t y) {
  if (y < THUMB_Y || y > THUMB_Y + THUMB_H) return;

  if (mediaDragStartX < 0) {
    mediaDragStartX  = x;
    mediaScrollStart = galleryScrollX;
    mediaDidDrag     = false;
    return;
  }

  int16_t delta = mediaDragStartX - x;
  if (abs(delta) > 5) mediaDidDrag = true;

  if (mediaDidDrag) {
    int16_t maxScroll = max((int16_t)0,
      (int16_t)(paintingCount * (THUMB_W + THUMB_PAD) - SCREEN_W + PANEL_CONTENT_X));
    int16_t newScroll = constrain(mediaScrollStart + delta, 0, maxScroll);
    if (newScroll != galleryScrollX) {
      galleryScrollX = newScroll;
      drawMediaPanel();
    }
  }
}

void handleMediaRelease(int16_t x, int16_t y) {
  Serial0.print("MediaRelease x:"); Serial0.print(x);
  Serial0.print(" y:"); Serial0.print(y);
  Serial0.print(" didDrag:"); Serial0.print(mediaDidDrag);
  Serial0.print(" THUMB_Y:"); Serial0.print(THUMB_Y);
  Serial0.print(" THUMB_Y+H:"); Serial0.println(THUMB_Y + THUMB_H);

  if (mediaDragStartX >= 0 && !mediaDidDrag) {
    if (y >= THUMB_Y && y <= THUMB_Y + THUMB_H) {
      int16_t idx = (x + galleryScrollX - PANEL_CONTENT_X) / (THUMB_W + THUMB_PAD);
      Serial0.print("Tapped thumb idx:"); Serial0.println(idx);
      if (idx >= 0 && idx < (int16_t)paintingCount) {
        loadPainting((uint8_t)idx);
        closePanel();   // close media panel after loading
      }
    }
  }
  mediaDragStartX = -1;
  mediaDidDrag    = false;
}

