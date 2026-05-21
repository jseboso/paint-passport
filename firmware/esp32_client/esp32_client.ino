#include "lgfx_config.h"
#include "config.h"
#include "canvas.h"
#include "media.h"

// ─────────────────────────────────────────────
//  Display instance
// ─────────────────────────────────────────────
LGFX display;

// ─────────────────────────────────────────────
//  Shared state
// ─────────────────────────────────────────────
Panel    activePanel  = PANEL_NONE;
uint32_t currentColor = 0;
uint8_t  brushSize    = 5;
float    cpHue        = 0.0f;
float    cpSat        = 255.0f;
float    cpVal        = 200.0f;

uint32_t COL_WHITE, COL_BLACK, COL_DARKGREY;
uint32_t COL_MIDGREY, COL_HIGHLIGHT;
uint32_t COL_TOOLBAR_BG, COL_BTN_ACTIVE;

// ─────────────────────────────────────────────
//  Touch state
// ─────────────────────────────────────────────
static bool     wasTouching  = false;
static int16_t  lastX        = -1;
static int16_t  lastY        = -1;
static bool     drawing      = false;
static uint16_t lastSvCurX   = 0;
static uint16_t lastSvCurY   = 0;
static uint16_t lastThumbX   = 0;
static int16_t  mediaLastX   = -1;
static int16_t  mediaLastY   = -1;

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
void drawPanelBase(const char* title);
void drawCloseButton();
void drawToolbar();
void drawColorPanel();
void drawSizePanel();
void drawSettingsPanel();
void closePanel();
void openPanel(Panel p);

// ─────────────────────────────────────────────
//  UI color setup
// ─────────────────────────────────────────────
void buildUIColors() {
  COL_WHITE      = rgb(255, 255, 255);
  COL_BLACK      = rgb(0,   0,   0);
  COL_DARKGREY   = rgb(40,  40,  40);
  COL_MIDGREY    = rgb(90,  90,  90);
  COL_HIGHLIGHT  = rgb(130, 130, 130);
  COL_TOOLBAR_BG = rgb(25,  25,  25);
  COL_BTN_ACTIVE = rgb(60,  120, 220);
}

// ─────────────────────────────────────────────
//  HSV helper
// ─────────────────────────────────────────────
uint32_t hsvToColor(float h, float s, float v) {
  s /= 255.0f; v /= 255.0f;
  float c = v * s;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = v - c;
  float r, g, b;
  if      (h < 60)  { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else              { r = c; g = 0; b = x; }
  return rgb((uint8_t)((r+m)*255), (uint8_t)((g+m)*255), (uint8_t)((b+m)*255));
}

// ─────────────────────────────────────────────
//  Panel base chrome
// ─────────────────────────────────────────────
void drawPanelBase(const char* title) {
  display.fillRect(0, TOOLBAR_Y, SCREEN_W, TOOLBAR_H, COL_DARKGREY);
  display.drawRect(0, TOOLBAR_Y, SCREEN_W, TOOLBAR_H, COL_MIDGREY);
  uint32_t titleBg = rgb(20, 20, 20);
  display.fillRect(0, TOOLBAR_Y, SCREEN_W, PANEL_TITLE_H, titleBg);
  display.setTextColor(COL_WHITE, titleBg);
  display.setTextSize(2);
  display.setCursor(CP_PAD, TOOLBAR_Y + 9);
  display.print(title);
  display.drawFastHLine(0, TOOLBAR_Y + PANEL_TITLE_H, SCREEN_W, COL_MIDGREY);
  drawCloseButton();
}

// ─────────────────────────────────────────────
//  Close button
// ─────────────────────────────────────────────
void drawCloseButton() {
  uint32_t red = rgb(200, 40, 40);
  display.fillRoundRect(CLOSE_BTN_X, CLOSE_BTN_Y,
                        CLOSE_BTN_SIZE, CLOSE_BTN_SIZE, 8, red);
  uint16_t pad = 12;
  display.drawLine(CLOSE_BTN_X + pad, CLOSE_BTN_Y + pad,
                   CLOSE_BTN_X + CLOSE_BTN_SIZE - pad,
                   CLOSE_BTN_Y + CLOSE_BTN_SIZE - pad, COL_WHITE);
  display.drawLine(CLOSE_BTN_X + CLOSE_BTN_SIZE - pad, CLOSE_BTN_Y + pad,
                   CLOSE_BTN_X + pad,
                   CLOSE_BTN_Y + CLOSE_BTN_SIZE - pad, COL_WHITE);
  display.drawLine(CLOSE_BTN_X + pad + 1, CLOSE_BTN_Y + pad,
                   CLOSE_BTN_X + CLOSE_BTN_SIZE - pad + 1,
                   CLOSE_BTN_Y + CLOSE_BTN_SIZE - pad, COL_WHITE);
  display.drawLine(CLOSE_BTN_X + CLOSE_BTN_SIZE - pad + 1, CLOSE_BTN_Y + pad,
                   CLOSE_BTN_X + pad + 1,
                   CLOSE_BTN_Y + CLOSE_BTN_SIZE - pad, COL_WHITE);
}

// ─────────────────────────────────────────────
//  Toolbar
// ─────────────────────────────────────────────
const char* BTN_LABELS[] = { "Color", "Size", "Undo", "Media", "Settings" };

void drawToolbar() {
  display.fillRect(0, TOOLBAR_Y, SCREEN_W, TOOLBAR_H, COL_TOOLBAR_BG);
  display.drawFastHLine(0, TOOLBAR_Y, SCREEN_W, COL_MIDGREY);

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    uint16_t bx  = i * BTN_W;
    uint16_t bcx = bx + BTN_W / 2;
    bool active = (i == 0 && activePanel == PANEL_COLOR)    ||
                  (i == 1 && activePanel == PANEL_SIZE)      ||
                  (i == 3 && activePanel == PANEL_MEDIA)     ||
                  (i == 4 && activePanel == PANEL_SETTINGS);
    uint32_t bg = active ? COL_BTN_ACTIVE : COL_TOOLBAR_BG;
    uint32_t fg = COL_WHITE;

    display.fillRect(bx + 1, TOOLBAR_Y + 1, BTN_W - 2, TOOLBAR_H - 2, bg);
    if (i > 0) display.drawFastVLine(bx, TOOLBAR_Y, TOOLBAR_H, COL_MIDGREY);

    uint16_t iconY = TOOLBAR_Y + 55;
    uint16_t labelY = TOOLBAR_Y + 155;

    if (i == 0) {
      display.fillCircle(bcx, iconY, 20, currentColor);
      display.drawCircle(bcx, iconY, 20, fg);
      display.drawCircle(bcx, iconY, 21, fg);
    }
    else if (i == 1) {
      uint8_t r = min((int)brushSize, 18);
      display.fillCircle(bcx, iconY, r, fg);
    }
    else if (i == 2) {
      display.drawFastHLine(bcx - 14, iconY, 24, fg);
      display.drawLine(bcx - 14, iconY, bcx - 6, iconY - 8, fg);
      display.drawLine(bcx - 14, iconY, bcx - 6, iconY + 8, fg);
      display.drawLine(bcx - 15, iconY, bcx - 7, iconY - 8, fg);
      display.drawLine(bcx - 15, iconY, bcx - 7, iconY + 8, fg);
      for (float a = -90; a <= 90; a += 5.0f) {
        float rad = a * 3.14159f / 180.0f;
        uint16_t ax = bcx + 10 + (uint16_t)(12.0f * cosf(rad));
        uint16_t ay = iconY    + (int16_t)(12.0f * sinf(rad));
        display.drawPixel(ax, ay, fg);
        display.drawPixel(ax + 1, ay, fg);
      }
    }
    else if (i == 3) {
      display.drawRect(bcx - 20, iconY - 15, 40, 30, fg);
      display.fillCircle(bcx - 8, iconY - 6, 4, fg);
      display.drawLine(bcx - 18, iconY + 13, bcx - 2, iconY - 2, fg);
      display.drawLine(bcx - 2,  iconY - 2,  bcx + 8, iconY + 6,  fg);
      display.drawLine(bcx + 4,  iconY + 4,  bcx + 18, iconY + 13, fg);
      display.drawFastHLine(bcx - 18, iconY + 13, 36, fg);
    }
    else if (i == 4) {
      display.drawCircle(bcx, iconY, 10, fg);
      display.fillCircle(bcx, iconY, 6, bg);
      display.drawCircle(bcx, iconY, 6, fg);
      for (uint8_t t = 0; t < 8; t++) {
        float ang = t * 45.0f * 3.14159f / 180.0f;
        uint16_t x1 = bcx + (uint16_t)(10.0f * cosf(ang));
        uint16_t y1 = iconY + (int16_t)(10.0f * sinf(ang));
        uint16_t x2 = bcx + (uint16_t)(18.0f * cosf(ang));
        uint16_t y2 = iconY + (int16_t)(18.0f * sinf(ang));
        display.drawLine(x1, y1, x2, y2, fg);
        display.drawLine(x1+1, y1, x2+1, y2, fg);
      }
    }

    display.setTextColor(fg, bg);
    display.setTextSize(2);
    uint16_t tw = strlen(BTN_LABELS[i]) * 12;
    display.setCursor(bx + (BTN_W - tw) / 2, labelY);
    display.print(BTN_LABELS[i]);
  }
}

// ─────────────────────────────────────────────
//  Color panel
// ─────────────────────────────────────────────
void drawColorPanel() {
  drawPanelBase("Color");

  for (uint16_t i = 0; i < HUE_W; i++) {
    float h = (float)i / HUE_W * 360.0f;
    display.drawFastVLine(HUE_X + i, HUE_Y, HUE_H, hsvToColor(h, 255, 255));
  }
  uint16_t hueCurX = HUE_X + (uint16_t)(cpHue / 360.0f * HUE_W);
  display.drawRect(hueCurX - 2, HUE_Y - 2, 5, HUE_H + 4, COL_WHITE);

  for (uint16_t col = 0; col < SV_W; col++) {
    float s = (float)col / SV_W * 255.0f;
    for (uint16_t row = 0; row < SV_H; row++) {
      float v = (1.0f - (float)row / SV_H) * 255.0f;
      display.drawPixel(SV_X + col, SV_Y + row, hsvToColor(cpHue, s, v));
    }
  }
  uint16_t svCurX = SV_X + (uint16_t)(cpSat / 255.0f * SV_W);
  uint16_t svCurY = SV_Y + (uint16_t)((1.0f - cpVal / 255.0f) * SV_H);
  display.drawCircle(svCurX, svCurY, 6, COL_WHITE);
  display.drawCircle(svCurX, svCurY, 7, COL_BLACK);
  lastSvCurX = svCurX;
  lastSvCurY = svCurY;

  display.fillRect(PRV_X, PRV_Y, PRV_W, PRV_H, currentColor);
  display.drawRect(PRV_X, PRV_Y, PRV_W, PRV_H, COL_MIDGREY);
}

// ─────────────────────────────────────────────
//  Size panel
// ─────────────────────────────────────────────
void drawSizePanel() {
  drawPanelBase("Brush Size");

  display.setTextColor(COL_WHITE, COL_DARKGREY);
  display.setTextSize(3);
  char buf[8];
  snprintf(buf, sizeof(buf), "%dpx", brushSize);
  uint16_t tw = strlen(buf) * 18;
  display.setCursor(PANEL_CONTENT_X + PANEL_CONTENT_W - tw, PANEL_CONTENT_Y + 4);
  display.print(buf);

  uint32_t trackCol = rgb(70, 70, 70);
  display.fillRoundRect(SP_TRACK_X1, SP_TRACK_Y - 6, SP_TRACK_W, 12, 6, trackCol);
  uint16_t fillW = map(brushSize, BRUSH_MIN, BRUSH_MAX, 0, SP_TRACK_W);
  if (fillW > 0) {
    display.fillRoundRect(SP_TRACK_X1, SP_TRACK_Y - 6, fillW, 12, 6, COL_BTN_ACTIVE);
  }
  uint16_t thumbX = SP_TRACK_X1 + fillW;
  display.fillCircle(thumbX, SP_TRACK_Y, 18, COL_WHITE);
  display.drawCircle(thumbX, SP_TRACK_Y, 18, COL_MIDGREY);
  lastThumbX = thumbX;
}

// ─────────────────────────────────────────────
//  Settings panel
// ─────────────────────────────────────────────
void drawSettingsPanel() {
  drawPanelBase("Settings");

  uint32_t clrBg = rgb(180, 40, 40);
  display.fillRoundRect(PANEL_CONTENT_X, PANEL_CONTENT_Y + 10, 200, 50, 6, clrBg);
  display.setTextColor(COL_WHITE, clrBg);
  display.setTextSize(2);
  display.setCursor(PANEL_CONTENT_X + 10, PANEL_CONTENT_Y + 27);
  display.print("Clear Canvas");
}

// ─────────────────────────────────────────────
//  Panel management
// ─────────────────────────────────────────────
void closePanel() {
  activePanel = PANEL_NONE;
  drawToolbar();
}

void openPanel(Panel p) {
  activePanel = (activePanel == p) ? PANEL_NONE : p;
  mediaChromeDrawn = false;
  mediaLastX = -1;
  mediaLastY = -1;
  drawToolbar();
  switch (activePanel) {
    case PANEL_COLOR:    drawColorPanel();    break;
    case PANEL_SIZE:     drawSizePanel();     break;
    case PANEL_MEDIA:    drawMediaPanel();    break;
    case PANEL_SETTINGS: drawSettingsPanel(); break;
    default: break;
  }
}

// ─────────────────────────────────────────────
//  Touch handling
// ─────────────────────────────────────────────
void handleTouch() {
  lgfx::touch_point_t tp;
  bool isTouching = display.getTouch(&tp, 1) > 0;

  // ── Finger lifted ────────────────────────────
  if (!isTouching) {
    if (drawing) {
      drawing = false;
      if (strokeBuf[activeStroke].count > 0) {
        strokeCount = (uint8_t)min((int)strokeCount + 1, (int)MAX_STROKES);
      }
    }
    if (activePanel == PANEL_MEDIA && mediaLastX >= 0) {
      handleMediaRelease(mediaLastX, mediaLastY);
      mediaLastX = -1;
      mediaLastY = -1;
    }
    lastX = lastY = -1;
    wasTouching = false;
    return;
  }

  int16_t x = tp.x;
  int16_t y = tp.y;

  // ── Close button ─────────────────────────────
  if (activePanel != PANEL_NONE &&
      x >= CLOSE_BTN_X && x <= CLOSE_BTN_X + CLOSE_BTN_SIZE &&
      y >= CLOSE_BTN_Y && y <= CLOSE_BTN_Y + CLOSE_BTN_SIZE) {
    if (!wasTouching) {
      wasTouching = true;
      closePanel();
    }
    return;
  }

  // ── Continuous: color picker drag ────────────
  if (activePanel == PANEL_COLOR && y >= TOOLBAR_Y) {
    bool hueChanged = false;
    bool svChanged  = false;

    if (y >= HUE_Y && y <= HUE_Y + HUE_H + 10) {
      float newHue = constrain((float)(x - HUE_X) / HUE_W * 360.0f, 0.0f, 359.9f);
      if (fabsf(newHue - cpHue) > 0.5f) {
        cpHue = newHue;
        hueChanged = true;
      }
    } else if (y >= SV_Y - 10 && y <= SV_Y + SV_H + 10 &&
               x >= SV_X      && x <= SV_X + SV_W) {
      float newSat = constrain((float)(x - SV_X) / SV_W * 255.0f, 0.0f, 255.0f);
      float newVal = constrain((1.0f - (float)(y - SV_Y) / SV_H) * 255.0f, 0.0f, 255.0f);
      if (fabsf(newSat - cpSat) > 1.0f || fabsf(newVal - cpVal) > 1.0f) {
        cpSat = newSat;
        cpVal = newVal;
        svChanged = true;
      }
    }

    if (hueChanged || svChanged) {
      currentColor = hsvToColor(cpHue, cpSat, cpVal);
      if (hueChanged) {
        drawColorPanel();
      } else {
        for (int16_t dx = -8; dx <= 8; dx++) {
          for (int16_t dy = -8; dy <= 8; dy++) {
            int16_t px = lastSvCurX + dx;
            int16_t py = lastSvCurY + dy;
            if (px >= SV_X && px < (int16_t)(SV_X + SV_W) &&
                py >= SV_Y && py < (int16_t)(SV_Y + SV_H)) {
              float s = (float)(px - SV_X) / SV_W * 255.0f;
              float v = (1.0f - (float)(py - SV_Y) / SV_H) * 255.0f;
              display.drawPixel(px, py, hsvToColor(cpHue, s, v));
            }
          }
        }
        uint16_t curX = SV_X + (uint16_t)(cpSat / 255.0f * SV_W);
        uint16_t curY = SV_Y + (uint16_t)((1.0f - cpVal / 255.0f) * SV_H);
        display.drawCircle(curX, curY, 6, COL_WHITE);
        display.drawCircle(curX, curY, 7, COL_BLACK);
        lastSvCurX = curX;
        lastSvCurY = curY;
        display.fillRect(PRV_X, PRV_Y, PRV_W, PRV_H, currentColor);
        display.drawRect(PRV_X, PRV_Y, PRV_W, PRV_H, COL_MIDGREY);
        display.fillRect((BTN_W/2) - 16, TOOLBAR_Y + 20, 32, 32, currentColor);
        display.drawRect((BTN_W/2) - 16, TOOLBAR_Y + 20, 32, 32, COL_MIDGREY);
      }
    }
    return;
  }

  // ── Continuous: size slider drag ─────────────
  if (activePanel == PANEL_SIZE && y >= TOOLBAR_Y) {
    uint8_t newSize = (uint8_t)map(
      constrain(x - SP_TRACK_X1, 0, (int)SP_TRACK_W),
      0, SP_TRACK_W, BRUSH_MIN, BRUSH_MAX
    );
    if (newSize != brushSize) {
      brushSize = newSize;
      drawSizePanel();
    }
    return;
  }

  // ── Single-fire: media save button ───────────
  if (!wasTouching && activePanel == PANEL_MEDIA && y >= TOOLBAR_Y) {
    uint16_t saveBtnX = PANEL_CONTENT_X + PANEL_CONTENT_W - 90;
    uint16_t saveBtnY = PANEL_CONTENT_Y - PANEL_TITLE_H + 4;
    if (x >= (int16_t)saveBtnX && x <= (int16_t)(saveBtnX + 88) &&
        y >= (int16_t)saveBtnY && y <= (int16_t)(saveBtnY + 28)) {
      wasTouching = true;
      if (sdAvailable()) {
        savePainting();
        mediaChromeDrawn = false;
        drawMediaPanel();
      }
      return;
    }
  }

  // ── Continuous: media panel drag ─────────────
  if (activePanel == PANEL_MEDIA && y >= TOOLBAR_Y) {
    mediaLastX = x;
    mediaLastY = y;
    handleMediaTouch(x, y);
    return;
  }

  // ── Single-fire on touch down ─────────────────
  if (!wasTouching) {
    wasTouching = true;

    // Settings panel actions
    if (activePanel == PANEL_SETTINGS && y >= TOOLBAR_Y) {
      if (x >= PANEL_CONTENT_X && x <= PANEL_CONTENT_X + 200 &&
          y >= PANEL_CONTENT_Y + 10 && y <= PANEL_CONTENT_Y + 60) {
        clearCanvas();
        drawSettingsPanel();
      }
      return;
    }

    // Toolbar buttons
    if (y >= TOOLBAR_Y) {
      uint8_t btn = constrain(x / BTN_W, 0, NUM_BUTTONS - 1);
      switch (btn) {
        case 0: openPanel(PANEL_COLOR);    break;
        case 1: openPanel(PANEL_SIZE);     break;
        case 2: undoLastStroke();          break;
        case 3: openPanel(PANEL_MEDIA);    break;
        case 4: openPanel(PANEL_SETTINGS); break;
      }
      return;
    }
  }

  // ── Continuous: draw on canvas ────────────────
  if (y < TOOLBAR_Y) {
    if (!drawing) {
      drawing      = true;
      activeStroke = strokeCount % MAX_STROKES;
      strokeBuf[activeStroke].count = 0;
      strokeBuf[activeStroke].color = currentColor;
      strokeBuf[activeStroke].size  = brushSize;
    }

    Stroke& st = strokeBuf[activeStroke];
    if (st.count < MAX_POINTS) {
      st.pts[st.count++] = {x, y};
    }

    if (lastX >= 0 && lastY >= 0) {
      display.drawWideLine(lastX, lastY, x, y, brushSize, currentColor);
    } else {
      display.fillCircle(x, y, brushSize / 2, currentColor);
    }

    lastX = x;
    lastY = y;
  }
}

// ─────────────────────────────────────────────
//  Setup & Loop
// ─────────────────────────────────────────────
void setup() {
  Serial0.begin(115200);
  delay(1000);

  display.init();
  buildUIColors();
  currentColor = rgb(0, 0, 0);

  display.setRotation(1);
  display.setBrightness(200);
  display.fillScreen(COL_WHITE);

  bool sdOk = initSD();
  Serial0.print("SD: ");
  Serial0.println(sdOk ? "OK" : "not found");

  initCanvas();
  drawCanvas();
  drawToolbar();
  Serial0.println("Ready.");
}

void loop() {
  handleTouch();
  delay(8);
}
