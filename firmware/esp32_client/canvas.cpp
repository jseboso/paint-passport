#include "canvas.h"

Stroke  strokeBuf[MAX_STROKES];
uint8_t strokeCount  = 0;
uint8_t activeStroke = 0;

LGFX_Sprite undoSprite(&display);
bool        undoSpriteReady = false;

void initCanvas() {
  strokeCount = 0;

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

// Replay directly to display — used by clearCanvas
void replayStrokes() {
  display.fillRect(0, 0, CANVAS_W, CANVAS_H, COL_WHITE);
  for (uint8_t s = 0; s < strokeCount; s++) {
    Stroke& st = strokeBuf[s];
    if (st.count == 0) continue;
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
}

void undoLastStroke() {
  if (strokeCount == 0) return;
  strokeCount--;

  if (undoSpriteReady) {
    // Replay all remaining strokes into the sprite (fast — RAM only)
    undoSprite.fillSprite(COL_WHITE);
    for (uint8_t s = 0; s < strokeCount; s++) {
      Stroke& st = strokeBuf[s];
      if (st.count == 0) continue;
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
    // Push completed result to display in one blit
    undoSprite.pushSprite(0, 0);
  } else {
    // Fallback: replay directly to display
    replayStrokes();
  }
}

void clearCanvas() {
  strokeCount = 0;
  if (undoSpriteReady) {
    undoSprite.fillSprite(COL_WHITE);
    undoSprite.pushSprite(0, 0);  // only pushes CANVAS_W x CANVAS_H
  } else {
    display.fillRect(0, 0, CANVAS_W, CANVAS_H, COL_WHITE);
  }
}
