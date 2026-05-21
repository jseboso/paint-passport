#include "keyboard.h"
#include <string.h>
#include <ctype.h>

//  Key layout
// row 3 (shift/space/done) isn't simple characters, built directly in buildLayout()
static const char* ROW_LETTERS[3] = {
  "qwertyuiop",
  "asdfghjkl",
  "zxcvbnm"
};

// common chars
static const char* ROW_SYMBOLS[3] = {
  "1234567890",
  "!@#$%^&*()",
  "-_=+.,'"
};

enum KeyAction { KEY_CHAR, KEY_SHIFT, KEY_BACKSPACE, KEY_TOGGLE_SYMBOLS, KEY_SPACE, KEY_DONE };

struct KeyDef {
  int16_t   x, y, w, h;
  KeyAction action;
  char      ch;        // valid when action == KEY_CHAR or KEY_SPACE
  char      label[6];  // what to draw on the key
};

// used by both kbDraw() and kbHandleTouch() so position and tap target can't disagree
static uint8_t buildLayout(const Keyboard& kb, KeyDef out[32]) {
  uint8_t n = 0;
  const char* const* rows = kb.symbols ? ROW_SYMBOLS : ROW_LETTERS;
  uint16_t rowWidth = SCREEN_W - 2 * KB_GAP;
  uint16_t unit = rowWidth / 10;  // 10-unit grid every row aligns to
  uint16_t y = kb.top;

  // Row 0: 10 single-unit keys
  {
    uint16_t x = KB_GAP + (rowWidth - unit * 10) / 2;
    for (uint8_t i = 0; i < 10; i++) {
      char c = rows[0][i];
      if (kb.shifted && !kb.symbols) c = (char)toupper(c);
      out[n].x = x; out[n].y = y; out[n].w = unit - KB_GAP; out[n].h = KB_KEY_H;
      out[n].action = KEY_CHAR; out[n].ch = c;
      out[n].label[0] = c; out[n].label[1] = '\0';
      n++;
      x = (uint16_t)(x + unit);
    }
    y = (uint16_t)(y + KB_KEY_H + KB_GAP);
  }

  // Row 1: however many keys this layer has, single-unit width, centered
  {
    uint8_t count = (uint8_t)strlen(rows[1]);
    uint16_t rowW = unit * count;
    uint16_t x = (uint16_t)(KB_GAP + (rowWidth - rowW) / 2);
    for (uint8_t i = 0; i < count; i++) {
      char c = rows[1][i];
      if (kb.shifted && !kb.symbols) c = (char)toupper(c);
      out[n].x = x; out[n].y = y; out[n].w = unit - KB_GAP; out[n].h = KB_KEY_H;
      out[n].action = KEY_CHAR; out[n].ch = c;
      out[n].label[0] = c; out[n].label[1] = '\0';
      n++;
      x = (uint16_t)(x + unit);
    }
    y = (uint16_t)(y + KB_KEY_H + KB_GAP);
  }

  // shift (1.5u) + up to 7 chars (1u each) + backspace (1.5u) = 10u
  // shift key stays present in the symbols layer for layout consistency,
  // even though it has no visible effect on punctuation
  {
    uint16_t x = KB_GAP;
    uint16_t wideW = (uint16_t)(unit * 3 / 2 - KB_GAP);

    out[n].x = x; out[n].y = y; out[n].w = wideW; out[n].h = KB_KEY_H;
    out[n].action = KEY_SHIFT;
    strncpy(out[n].label, "SHIFT", sizeof(out[n].label) - 1);
    out[n].label[sizeof(out[n].label) - 1] = '\0';
    n++;
    x = (uint16_t)(x + unit * 3 / 2);

    uint8_t count = (uint8_t)strlen(rows[2]);
    for (uint8_t i = 0; i < count; i++) {
      char c = rows[2][i];
      if (kb.shifted && !kb.symbols) c = (char)toupper(c);
      out[n].x = x; out[n].y = y; out[n].w = unit - KB_GAP; out[n].h = KB_KEY_H;
      out[n].action = KEY_CHAR; out[n].ch = c;
      out[n].label[0] = c; out[n].label[1] = '\0';
      n++;
      x = (uint16_t)(x + unit);
    }

    out[n].x = x; out[n].y = y; out[n].w = wideW; out[n].h = KB_KEY_H;
    out[n].action = KEY_BACKSPACE;
    strncpy(out[n].label, "DEL", sizeof(out[n].label) - 1);
    out[n].label[sizeof(out[n].label) - 1] = '\0';
    n++;
    y = (uint16_t)(y + KB_KEY_H + KB_GAP);
  }

  // Row 3: layer-toggle (1.5u) + space (6u) + done (2.5u) = 10u
  {
    uint16_t x = KB_GAP;
    uint16_t toggleW = (uint16_t)(unit * 3 / 2 - KB_GAP);
    uint16_t spaceW  = (uint16_t)(unit * 6 - KB_GAP);
    uint16_t doneW   = (uint16_t)(unit * 5 / 2 - KB_GAP);

    out[n].x = x; out[n].y = y; out[n].w = toggleW; out[n].h = KB_KEY_H;
    out[n].action = KEY_TOGGLE_SYMBOLS;
    strncpy(out[n].label, kb.symbols ? "ABC" : "123", sizeof(out[n].label) - 1);
    out[n].label[sizeof(out[n].label) - 1] = '\0';
    n++;
    x = (uint16_t)(x + unit * 3 / 2);

    out[n].x = x; out[n].y = y; out[n].w = spaceW; out[n].h = KB_KEY_H;
    out[n].action = KEY_SPACE; out[n].ch = ' ';
    out[n].label[0] = '\0';
    n++;
    x = (uint16_t)(x + unit * 6);

    out[n].x = x; out[n].y = y; out[n].w = doneW; out[n].h = KB_KEY_H;
    out[n].action = KEY_DONE;
    strncpy(out[n].label, "DONE", sizeof(out[n].label) - 1);
    out[n].label[sizeof(out[n].label) - 1] = '\0';
    n++;
  }

  return n;
}

void kbDraw(Keyboard& kb) {
  KeyDef keys[32];
  uint8_t n = buildLayout(kb, keys);

  display.fillRect(0, kb.top - KB_GAP, SCREEN_W, KB_H + KB_GAP, COL_TOOLBAR_BG);

  for (uint8_t i = 0; i < n; i++) {
    KeyDef& k = keys[i];
    bool active = (k.action == KEY_SHIFT && kb.shifted);
    uint32_t bg = active ? COL_BTN_ACTIVE : COL_DARKGREY;
    display.fillRoundRect(k.x, k.y, k.w, k.h, 6, bg);
    display.drawRoundRect(k.x, k.y, k.w, k.h, 6, COL_MIDGREY);

    if (k.label[0] == '\0') continue;  // space bar: no label
    bool isLetterKey = (k.action == KEY_CHAR);
    display.setTextColor(COL_WHITE, bg);
    display.setTextSize(isLetterKey ? 3 : 2);
    uint8_t charW = isLetterKey ? 18 : 12;
    uint16_t tw = (uint16_t)(strlen(k.label) * charW);
    display.setCursor(k.x + (k.w - tw) / 2, k.y + k.h / 2 - (isLetterKey ? 9 : 6));
    display.print(k.label);
  }
}

void kbDrawField(const Keyboard& kb, uint16_t fx, uint16_t fy, uint16_t fw, uint16_t fh,
                  const char* placeholder) {
  display.fillRoundRect(fx, fy, fw, fh, 6, COL_DARKGREY);
  display.drawRoundRect(fx, fy, fw, fh, 6, COL_MIDGREY);

  size_t len = (kb.buf != nullptr) ? strlen(kb.buf) : 0;
  display.setTextSize(2);
  display.setCursor(fx + 10, fy + fh / 2 - 8);

  if (len == 0) {
    display.setTextColor(COL_MIDGREY, COL_DARKGREY);
    if (placeholder) display.print(placeholder);
    return;
  }

  display.setTextColor(COL_WHITE, COL_DARKGREY);
  if (kb.masked) {
    char dots[48];
    size_t n = len < sizeof(dots) - 1 ? len : sizeof(dots) - 1;
    for (size_t i = 0; i < n; i++) dots[i] = '*';
    dots[n] = '\0';
    display.print(dots);
  } else {
    display.print(kb.buf);
  }
}

void kbOpen(Keyboard& kb, char* buf, uint16_t bufSize, uint16_t top, bool masked) {
  kb.buf     = buf;
  kb.bufSize = bufSize;
  kb.masked  = masked;
  kb.shifted = false;
  kb.symbols = false;
  kb.top     = top;
  if (kb.buf != nullptr && kb.bufSize > 0) kb.buf[0] = '\0';
  kbDraw(kb);
}

KbResult kbHandleTouch(Keyboard& kb, int16_t x, int16_t y) {
  if (y < (int16_t)(kb.top - KB_GAP)) return KB_NONE;

  KeyDef keys[32];
  uint8_t n = buildLayout(kb, keys);

  for (uint8_t i = 0; i < n; i++) {
    KeyDef& k = keys[i];
    if (x < k.x || x >= k.x + k.w || y < k.y || y >= k.y + k.h) continue;

    switch (k.action) {
      case KEY_CHAR:
      case KEY_SPACE: {
        if (kb.buf == nullptr) return KB_NONE;
        size_t len = strlen(kb.buf);
        if (len + 1 < kb.bufSize) {
          kb.buf[len]     = k.ch;
          kb.buf[len + 1] = '\0';
        }
        return KB_EDITED;
      }
      case KEY_BACKSPACE: {
        if (kb.buf == nullptr) return KB_NONE;
        size_t len = strlen(kb.buf);
        if (len > 0) kb.buf[len - 1] = '\0';
        return KB_EDITED;
      }
      case KEY_SHIFT:
        kb.shifted = !kb.shifted;
        kbDraw(kb);
        return KB_EDITED;
      case KEY_TOGGLE_SYMBOLS:
        kb.symbols = !kb.symbols;
        kb.shifted = false;
        kbDraw(kb);
        return KB_EDITED;
      case KEY_DONE:
        return KB_DONE;
    }
    return KB_NONE;
  }
  return KB_NONE;
}
