#pragma once
#include "config.h"
#include <LovyanGFX.hpp>

static const uint8_t  MAX_STROKES = 10;
static const uint16_t MAX_POINTS  = 150;

struct Point  { int16_t x, y; };
struct Stroke {
  Point    pts[MAX_POINTS];
  uint16_t count;
  uint32_t color;
  uint8_t  size;
};

extern Stroke  strokeBuf[MAX_STROKES];
extern uint8_t strokeCount;
extern uint8_t activeStroke;

// PSRAM sprite used for undo replay
extern LGFX_Sprite undoSprite;
extern bool        undoSpriteReady;

void initCanvas();
void drawCanvas();
void replayStrokes();   // replays to display directly (used for clear)
void undoLastStroke();  // uses sprite for fast undo
void clearCanvas();
