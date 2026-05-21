#pragma once
#include "config.h"

//  Reusable on-screen QWERTY keyboard
static const uint16_t KB_KEY_H = 58;
static const uint16_t KB_GAP   = 4;
static const uint8_t  KB_ROWS  = 4;
static const uint16_t KB_H     = KB_ROWS * (KB_KEY_H + KB_GAP) + KB_GAP;

struct Keyboard {
  char*    buf     = nullptr;  // text buffer being edited (caller-owned)
  uint16_t bufSize = 0;        // capacity of buf, including the null terminator
  bool     masked  = false;    // true = render the field's contents as bullets
  bool     shifted = false;    // letters layer: uppercase toggle
  bool     symbols = false;    // symbols layer instead of letters
  uint16_t top     = 0;        // screen y where the keyboard's top row starts
};

enum KbResult { KB_NONE, KB_EDITED, KB_DONE };

// clears buf, resets layer/shift state, draws immediately
void kbOpen(Keyboard& kb, char* buf, uint16_t bufSize, uint16_t top, bool masked);

// redraws the keyboard itself; kbHandleTouch() calls this on its own when needed
void kbDraw(Keyboard& kb);

// draws the text field above the keyboard (bullets if masked, placeholder if empty)
void kbDrawField(const Keyboard& kb, uint16_t fieldX, uint16_t fieldY,
                  uint16_t fieldW, uint16_t fieldH, const char* placeholder);

// call on a fresh touch-down; KB_EDITED means refresh the field, KB_DONE means Enter was hit
KbResult kbHandleTouch(Keyboard& kb, int16_t x, int16_t y);
