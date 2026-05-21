#include "canvas.h"
#include "media.h"  // for loadBMPToSprite()

Stroke   strokeBuf[MAX_STROKES];
uint32_t nextStrokeIndex = 0;
uint8_t  strokeCount     = 0;
uint8_t  activeStroke    = 0;
char     loadedBaselinePath[24] = "";

LGFX_Sprite undoSprite(&display);
bool        undoSpriteReady = false;

void initCanvas() {
  nextStrokeIndex = 0;
  strokeCount = 0;
  loadedBaselinePath[0] = '\0';

  // Try to create sprite in PSRAM
  undoSprite.setPsram(true);
  undoSprite.setColorDepth(16);
  undoSpriteReady = undoSprite.createSprite(CANVAS_W, CANVAS_H);

  if (undoSpriteReady) {
    undoSprite.fillSprite(COL_WHITE);
    Serial0.println("Undo sprite: OK");
  } else {
    Serial0.println("Undo sprite: FAILED (no PSRAM?)");
  }
}

void drawCanvas() {
  display.fillRect(0, 0, CANVAS_W, CANVAS_H, COL_WHITE);
  if (undoSpriteReady) {
    undoSprite.fillSprite(COL_WHITE);
  }
}

// Ring-buffer slot of the oldest currently-retained stroke.
static inline uint8_t oldestStrokeSlot() {
  return (uint8_t)((nextStrokeIndex - strokeCount) % MAX_STROKES);
}

// No-PSRAM fallback for undo/clear. Always starts from white
void replayStrokes() {
  display.fillRect(0, 0, CANVAS_W, CANVAS_H, COL_WHITE);
  uint8_t slot = oldestStrokeSlot();
  for (uint8_t i = 0; i < strokeCount; i++) {
    Stroke& st = strokeBuf[slot];
    if (st.count > 0) {
      display.fillCircle(st.pts[0].x, st.pts[0].y, st.size / 2, st.color);
      for (uint16_t p = 1; p < st.count; p++) {
        int16_t dx = st.pts[p].x - st.pts[p-1].x;
        int16_t dy = st.pts[p].y - st.pts[p-1].y;
        if (dx*dx + dy*dy < 4) continue;
        display.drawWideLine(
          st.pts[p-1].x, st.pts[p-1].y,
          st.pts[p].x,   st.pts[p].y,
          st.size, st.color
        );
      }
    }
    slot = (uint8_t)((slot + 1) % MAX_STROKES);
  }
}

void undoLastStroke() {
  if (strokeCount == 0) return;
  strokeCount--;
  nextStrokeIndex--;  // next new stroke overwrites this slot again

  if (undoSpriteReady) {
    // Reset to baseline, else white.
    if (loadedBaselinePath[0] != '\0') {
      loadBMPToSprite(loadedBaselinePath, undoSprite);
    } else {
      undoSprite.fillSprite(COL_WHITE);
    }

    // walk oldest -> newest; not slots 0..strokeCount-1
    uint8_t slot = oldestStrokeSlot();
    for (uint8_t i = 0; i < strokeCount; i++) {
      Stroke& st = strokeBuf[slot];
      if (st.count > 0) {
        undoSprite.fillCircle(st.pts[0].x, st.pts[0].y,
                              st.size / 2, st.color);
        for (uint16_t p = 1; p < st.count; p++) {
          int16_t dx = st.pts[p].x - st.pts[p-1].x;
          int16_t dy = st.pts[p].y - st.pts[p-1].y;
          if (dx*dx + dy*dy < 4) continue;
          undoSprite.drawWideLine(
            st.pts[p-1].x, st.pts[p-1].y,
            st.pts[p].x,   st.pts[p].y,
            st.size, st.color
          );
        }
      }
      slot = (uint8_t)((slot + 1) % MAX_STROKES);
    }
    // Push completed result to display in one blit
    undoSprite.pushSprite(0, 0);
  } else {
    // Fallback: replay directly to display
    replayStrokes();
  }
}

void clearCanvas() {
  nextStrokeIndex = 0;
  strokeCount = 0;
  loadedBaselinePath[0] = '\0';  // explicit clear
  if (undoSpriteReady) {
    undoSprite.fillSprite(COL_WHITE);
    undoSprite.pushSprite(0, 0);  // only pushes CANVAS_W x CANVAS_H
  } else {
    display.fillRect(0, 0, CANVAS_W, CANVAS_H, COL_WHITE);
  }
}
