#include "media.h"
#include "canvas.h"
#include "cloud.h"

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
static void cacheThumbnail(uint8_t index);
static void drawThumbnail(uint8_t index, uint16_t screenX);
static void drawMediaTabs();
static void drawUploadIcon(int16_t tx);
static void flashUploadIcon(int16_t tx, uint32_t color);
static bool uploadIconHit(int16_t tx, int16_t x, int16_t y);

// ─────────────────────────────────────────────
//  SD state
// ─────────────────────────────────────────────
static bool  _sdReady          = false;
uint8_t      paintingCount     = 0;
int16_t      galleryScrollX    = 0;
bool         mediaChromeDrawn  = false;
static char  paintingNames[MAX_PAINTINGS][24];

// ─────────────────────────────────────────────
//  Created vs. received gallery view
// ─────────────────────────────────────────────
enum MediaView { MEDIA_VIEW_CREATED, MEDIA_VIEW_RECEIVED };
static MediaView mediaView = MEDIA_VIEW_CREATED;

static bool isReceivedName(const char* path) {
  const char* slash = strrchr(path, '/');
  const char* base = slash ? slash + 1 : path;
  return base[0] == 'R';
}

// Fills out[] with the paintingNames[] indices belonging to the current
// view, in gallery order. Returns how many.
static uint8_t buildVisibleIndices(uint8_t* out) {
  uint8_t n = 0;
  bool wantReceived = (mediaView == MEDIA_VIEW_RECEIVED);
  for (uint8_t i = 0; i < paintingCount; i++) {
    if (isReceivedName(paintingNames[i]) == wantReceived) {
      out[n++] = i;
    }
  }
  return n;
}

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

  bool swapRB = isReceivedName(paintingNames[index]);

  t.seek(54);
  for (uint16_t row = 0; row < THUMB_H; row++) {
    uint16_t storeRow = THUMB_H - 1 - row;  // BMP is bottom-up
    for (uint16_t col = 0; col < THUMB_W; col++) {
      uint8_t b = t.read();
      uint8_t g = t.read();
      uint8_t r = t.read();
      thumbCache[index][storeRow * THUMB_W + col] =
          swapRB ? display.color565(b, g, r) : display.color565(r, g, b);
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

static int16_t uploadIconX(int16_t tx) { return tx + THUMB_W - UPLOAD_ICON_SIZE - 4; }
static int16_t uploadIconY() { return THUMB_Y + THUMB_H - UPLOAD_ICON_SIZE - 4; }

static void drawUploadIcon(int16_t tx) {
  int16_t  iconX = uploadIconX(tx);
  int16_t  iconY = uploadIconY();
  uint16_t r     = UPLOAD_ICON_SIZE / 2;
  int16_t  cx    = iconX + r;
  int16_t  cy    = iconY + r;

  display.fillCircle(cx, cy, r, COL_BTN_ACTIVE);
  display.drawCircle(cx, cy, r, COL_WHITE);
  // simple up-arrow: stem + chevron
  display.drawFastVLine(cx,     cy - 7, 11, COL_WHITE);
  display.drawFastVLine(cx + 1, cy - 7, 11, COL_WHITE);
  display.drawLine(cx - 5, cy - 2, cx,     cy - 8, COL_WHITE);
  display.drawLine(cx,     cy - 8, cx + 5, cy - 2, COL_WHITE);
}

// Solid-color flash used to show busy/success/failure while an upload runs.
static void flashUploadIcon(int16_t tx, uint32_t color) {
  int16_t  iconX = uploadIconX(tx);
  int16_t  iconY = uploadIconY();
  uint16_t r     = UPLOAD_ICON_SIZE / 2;
  display.fillCircle(iconX + r, iconY + r, r, color);
  display.drawCircle(iconX + r, iconY + r, r, COL_WHITE);
}

static bool uploadIconHit(int16_t tx, int16_t x, int16_t y) {
  int16_t iconX = uploadIconX(tx);
  int16_t iconY = uploadIconY();
  return x >= iconX && x <= iconX + (int16_t)UPLOAD_ICON_SIZE &&
         y >= iconY && y <= iconY + (int16_t)UPLOAD_ICON_SIZE;
}

bool uploadPaintingAtIndex(uint8_t index) {
  if (index >= paintingCount) return false;
  return cloudUploadPainting(paintingNames[index]);
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

  // See cacheThumbnail() for why received files need red/blue swapped back
  // at decode time.
  bool swapRB = isReceivedName(paintingNames[index]);

  f.seek(54);
  uint16_t buf[CANVAS_W];
  for (int16_t row = CANVAS_H - 1; row >= 0; row--) {
    for (uint16_t col = 0; col < CANVAS_W; col++) {
      uint8_t b = f.read();
      uint8_t g = f.read();
      uint8_t r = f.read();
      buf[col] = swapRB ? display.color565(b, g, r) : display.color565(r, g, b);
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

  // See cacheThumbnail() for why received files need red/blue swapped back
  // at decode time.
  bool swapRB = isReceivedName(path);

  f.seek(54);
  uint16_t buf[CANVAS_W];
  for (int16_t row = CANVAS_H - 1; row >= 0; row--) {
    for (uint16_t col = 0; col < CANVAS_W; col++) {
      uint8_t b = f.read();
      uint8_t g = f.read();
      uint8_t r = f.read();
      buf[col] = swapRB ? display.color565(b, g, r) : display.color565(r, g, b);
    }
    uint16_t pad = (4 - (CANVAS_W * 3) % 4) % 4;
    for (uint8_t p = 0; p < pad; p++) f.read();
    sprite.pushImage(0, row, CANVAS_W, 1, buf);
  }
  f.close();
  return true;
}

// ─────────────────────────────────────────────
//  Receiving a painting from the cloud inbox (see cloud.cpp)
// ─────────────────────────────────────────────
static char pendingInboxPath[24] = "";

static bool generateThumbnailFromFile(const char* fullPath, const char* thumbPath) {
  File src = SD.open(fullPath);
  if (!src) return false;

  File t = SD.open(thumbPath, FILE_WRITE);
  if (!t) {
    src.close();
    return false;
  }
  writeBMPHeader(t, THUMB_W, THUMB_H);

  const uint32_t rowSize = (uint32_t)CANVAS_W * 3;
  uint8_t rowBuf[CANVAS_W * 3];

  for (int16_t row = THUMB_H - 1; row >= 0; row--) {
    uint16_t srcY = (uint32_t)row * CANVAS_H / THUMB_H;
    uint32_t fileRowIndex = CANVAS_H - 1 - srcY;  // BMP rows are stored bottom-up
    src.seek(54 + fileRowIndex * rowSize);
    src.read(rowBuf, rowSize);

    for (uint16_t col = 0; col < THUMB_W; col++) {
      uint16_t srcX = (uint32_t)col * CANVAS_W / THUMB_W;
      t.write(&rowBuf[srcX * 3], 3);  // already stored as B,G,R
    }
    uint16_t pad = (4 - (THUMB_W * 3) % 4) % 4;
    for (uint8_t p = 0; p < pad; p++) t.write((uint8_t)0);
  }

  t.close();
  src.close();
  return true;
}

bool beginInboxReceive(char* outPath, size_t outPathLen) {
  if (!_sdReady || paintingCount >= MAX_PAINTINGS) return false;
  // "R" prefix marks this as a received (not locally-painted) file - see
  // isReceivedName() above.
  snprintf(pendingInboxPath, sizeof(pendingInboxPath), "/paintings/R%03d.bmp", paintingCount);
  snprintf(outPath, outPathLen, "%s", pendingInboxPath);
  return true;
}

bool finishInboxReceive() {
  if (!_sdReady || pendingInboxPath[0] == '\0') return false;

  uint8_t idx = paintingCount;
  char thumbPath[28];
  snprintf(thumbPath, sizeof(thumbPath), "/thumbnails/%03d.bmp", idx);

  if (!generateThumbnailFromFile(pendingInboxPath, thumbPath)) {
    Serial0.println("Cloud inbox: thumbnail generation failed (gallery entry still added)");
  }

  snprintf(paintingNames[paintingCount], sizeof(paintingNames[0]), "%s", pendingInboxPath);
  paintingCount++;
  thumbCached[idx] = false;
  cacheThumbnail(idx);

  pendingInboxPath[0] = '\0';
  return true;
}

// ─────────────────────────────────────────────
//  Draw media panel
// ─────────────────────────────────────────────
void drawMediaPanel() {
  if (!mediaChromeDrawn) {
    drawPanelBase("Media");
    drawMediaTabs();

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

  uint8_t visible[MAX_PAINTINGS];
  uint8_t visibleCount = buildVisibleIndices(visible);

  if (visibleCount == 0) {
    display.setTextColor(COL_MIDGREY, COL_DARKGREY);
    display.setTextSize(2);
    display.setCursor(PANEL_CONTENT_X, THUMB_Y + THUMB_H / 2 - 8);
    display.print(mediaView == MEDIA_VIEW_RECEIVED ? "No received images" : "No saved paintings");
  } else {
    for (uint8_t slot = 0; slot < visibleCount; slot++) {
      uint8_t i = visible[slot];
      int16_t tx = PANEL_CONTENT_X + slot * (THUMB_W + THUMB_PAD) - galleryScrollX;
      if (tx + THUMB_W <= 0 || tx >= SCREEN_W) continue;
      if (tx >= 0 && tx + THUMB_W <= SCREEN_W) {
        drawThumbnail(i, tx);
        display.drawRect(tx, THUMB_Y, THUMB_W, THUMB_H, COL_WHITE);
        if (mediaView == MEDIA_VIEW_CREATED) drawUploadIcon(tx);
      } else {
        int16_t visX = max((int16_t)0, tx);
        int16_t visW = min((int16_t)SCREEN_W, (int16_t)(tx + THUMB_W)) - visX;
        display.fillRect(visX, THUMB_Y, visW, THUMB_H, COL_MIDGREY);
      }
    }
  }
}

// Two small tabs in the title strip, between the "Media" label and the SAVE
// button, for switching between paintings you made and images sent from the
// web gallery.
static void drawMediaTabs() {
  uint32_t bgCreated  = (mediaView == MEDIA_VIEW_CREATED)  ? COL_BTN_ACTIVE : COL_MIDGREY;
  uint32_t bgReceived = (mediaView == MEDIA_VIEW_RECEIVED) ? COL_BTN_ACTIVE : COL_MIDGREY;

  display.fillRoundRect(MEDIA_TAB1_X, MEDIA_TAB_Y, MEDIA_TAB_W, MEDIA_TAB_H, 6, bgCreated);
  display.setTextColor(COL_WHITE, bgCreated);
  display.setTextSize(2);
  display.setCursor(MEDIA_TAB1_X + 8, MEDIA_TAB_Y + 6);
  display.print("Created");

  display.fillRoundRect(MEDIA_TAB2_X, MEDIA_TAB_Y, MEDIA_TAB_W, MEDIA_TAB_H, 6, bgReceived);
  display.setTextColor(COL_WHITE, bgReceived);
  display.setCursor(MEDIA_TAB2_X + 4, MEDIA_TAB_Y + 6);
  display.print("Received");
}

// Returns true (and switches view + redraws) if (x,y) hit one of the tabs.
// Called from esp32_client.ino's touch handler alongside the SAVE button
// hit-test, since both live in the same title-strip region.
bool handleMediaTabTouch(int16_t x, int16_t y) {
  if (y < (int16_t)MEDIA_TAB_Y || y > (int16_t)(MEDIA_TAB_Y + MEDIA_TAB_H)) return false;

  MediaView newView;
  if (x >= (int16_t)MEDIA_TAB1_X && x <= (int16_t)(MEDIA_TAB1_X + MEDIA_TAB_W)) {
    newView = MEDIA_VIEW_CREATED;
  } else if (x >= (int16_t)MEDIA_TAB2_X && x <= (int16_t)(MEDIA_TAB2_X + MEDIA_TAB_W)) {
    newView = MEDIA_VIEW_RECEIVED;
  } else {
    return false;
  }

  if (newView != mediaView) {
    mediaView = newView;
    galleryScrollX = 0;
    mediaChromeDrawn = false;  // repaint the tabs with the new active state
    drawMediaPanel();
  }
  return true;
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
    uint8_t visible[MAX_PAINTINGS];
    uint8_t visibleCount = buildVisibleIndices(visible);
    int16_t maxScroll = max((int16_t)0,
      (int16_t)(visibleCount * (THUMB_W + THUMB_PAD) - SCREEN_W + PANEL_CONTENT_X));
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
      uint8_t visible[MAX_PAINTINGS];
      uint8_t visibleCount = buildVisibleIndices(visible);
      int16_t slot = (x + galleryScrollX - PANEL_CONTENT_X) / (THUMB_W + THUMB_PAD);
      Serial0.print("Tapped slot:"); Serial0.println(slot);
      if (slot >= 0 && slot < (int16_t)visibleCount) {
        uint8_t i  = visible[slot];
        int16_t tx = PANEL_CONTENT_X + slot * (THUMB_W + THUMB_PAD) - galleryScrollX;

        if (mediaView == MEDIA_VIEW_CREATED && uploadIconHit(tx, x, y)) {
          // Tapped the upload icon instead of the thumbnail itself - send
          // this painting to the cloud without loading it onto the canvas.
          flashUploadIcon(tx, COL_MIDGREY);           // busy
          bool ok = uploadPaintingAtIndex(i);
          Serial0.println(ok ? "Cloud upload: ok" : "Cloud upload: failed");
          flashUploadIcon(tx, ok ? rgb(40, 140, 40) : rgb(180, 40, 40));
          delay(400);
          drawMediaPanel();  // restore normal chrome
        } else {
          loadPainting(i);
          closePanel();   // close media panel after loading
        }
      }
    }
  }
  mediaDragStartX = -1;
  mediaDidDrag    = false;
}
