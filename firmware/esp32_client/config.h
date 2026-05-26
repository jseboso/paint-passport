#pragma once
#include "lgfx_config.h"

// ─────────────────────────────────────────────
//  Display
// ─────────────────────────────────────────────
extern LGFX display;

inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return display.color888(r, g, b);
}

// ─────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────
static const uint16_t SCREEN_W    = 480;
static const uint16_t SCREEN_H    = 800;
static const uint16_t TOOLBAR_H   = 200;
static const uint16_t TOOLBAR_Y   = SCREEN_H - TOOLBAR_H;  // 600
static const uint16_t CANVAS_W    = SCREEN_W;
static const uint16_t CANVAS_H    = TOOLBAR_Y;
static const uint8_t  NUM_BUTTONS = 5;
static const uint16_t BTN_W       = SCREEN_W / NUM_BUTTONS; // 96

// Panel chrome
static const uint16_t PANEL_TITLE_H   = 36;
static const uint16_t PANEL_CONTENT_X = 12;
static const uint16_t PANEL_CONTENT_Y = TOOLBAR_Y + PANEL_TITLE_H + 4;

static const uint16_t CLOSE_BTN_SIZE = 28;
static const uint16_t CLOSE_BTN_X    = SCREEN_W - CLOSE_BTN_SIZE - 8;
static const uint16_t CLOSE_BTN_Y    = TOOLBAR_Y + (PANEL_TITLE_H - CLOSE_BTN_SIZE) / 2;

static const uint16_t PANEL_CONTENT_W = CLOSE_BTN_X - 12 - 8;
static const uint16_t PANEL_CONTENT_H = TOOLBAR_H - PANEL_TITLE_H - 16;

// Color panel
static const uint16_t CP_PAD  = PANEL_CONTENT_X;
static const uint16_t CP_Y    = TOOLBAR_Y;
static const uint16_t CP_H    = TOOLBAR_H;
static const uint16_t HUE_X   = PANEL_CONTENT_X;
static const uint16_t HUE_Y   = PANEL_CONTENT_Y;
static const uint16_t HUE_W   = PANEL_CONTENT_W;
static const uint16_t HUE_H   = 28;
static const uint16_t SV_X    = PANEL_CONTENT_X;
static const uint16_t SV_Y    = HUE_Y + HUE_H + 6;
static const uint16_t SV_W    = PANEL_CONTENT_W - 60;
static const uint16_t SV_H    = PANEL_CONTENT_Y + PANEL_CONTENT_H - SV_Y - 4;
static const uint16_t PRV_X   = SV_X + SV_W + 8;
static const uint16_t PRV_Y   = SV_Y;
static const uint16_t PRV_W   = CLOSE_BTN_X - PRV_X - 8;
static const uint16_t PRV_H   = SV_H;

// Size panel
static const uint16_t SP_H        = TOOLBAR_H;
static const uint16_t SP_Y        = TOOLBAR_Y;
static const uint16_t SP_PAD      = PANEL_CONTENT_X;
static const uint16_t SP_TRACK_Y  = PANEL_CONTENT_Y + PANEL_CONTENT_H / 2;
static const uint16_t SP_TRACK_X1 = PANEL_CONTENT_X;
static const uint16_t SP_TRACK_X2 = PANEL_CONTENT_X + PANEL_CONTENT_W;
static const uint16_t SP_TRACK_W  = SP_TRACK_X2 - SP_TRACK_X1;

// Settings panel
static const uint16_t SETP_H = TOOLBAR_H;
static const uint16_t SETP_Y = TOOLBAR_Y;

// Media panel
static const uint16_t MEDIA_Y       = TOOLBAR_Y;
static const uint16_t MEDIA_H       = TOOLBAR_H;
static const uint16_t THUMB_W       = 150;
static const uint16_t THUMB_H       = PANEL_CONTENT_H - 8;
static const uint16_t THUMB_PAD     = 8;
static const uint16_t THUMB_Y       = PANEL_CONTENT_Y;
static const uint8_t  MAX_PAINTINGS = 20;

static const uint16_t UPLOAD_ICON_SIZE = 34;

// Created/Received view tabs, in the title strip between the "Media" label
// and the SAVE button (x: ~12-72 for the label, ~346-434 for SAVE).
static const uint16_t MEDIA_TAB_Y    = TOOLBAR_Y + 8;
static const uint16_t MEDIA_TAB_H    = 28;
static const uint16_t MEDIA_TAB_W    = 112;
static const uint16_t MEDIA_TAB_GAP  = 6;
static const uint16_t MEDIA_TAB1_X   = 80;
static const uint16_t MEDIA_TAB2_X   = MEDIA_TAB1_X + MEDIA_TAB_W + MEDIA_TAB_GAP;

// Brush
static const uint8_t BRUSH_MIN = 1;
static const uint8_t BRUSH_MAX = 40;

// SD SPI pins
static const uint8_t SD_CS   = 10;
static const uint8_t SD_MOSI = 11;
static const uint8_t SD_SCK  = 12;
static const uint8_t SD_MISO = 13;

// ─────────────────────────────────────────────
//  Panel enum
// ─────────────────────────────────────────────
enum Panel { PANEL_NONE, PANEL_COLOR, PANEL_SIZE, PANEL_MEDIA, PANEL_SETTINGS };

// MODE_WIFI takes over entire display
enum AppMode { MODE_PAINT, MODE_WIFI };

// ─────────────────────────────────────────────
//  Shared state
// ─────────────────────────────────────────────
extern Panel    activePanel;
extern AppMode  appMode;
extern uint32_t currentColor;
extern uint8_t  brushSize;
extern float    cpHue, cpSat, cpVal;

// true once a touch may act on the active panel/WiFi screen
extern bool     panelArmed;

// true while a stroke is actively being drawn on the canvas - cloud.cpp uses
// this to avoid kicking off a multi-second inbox download mid-stroke.
extern bool     drawing;

extern uint32_t COL_WHITE, COL_BLACK, COL_DARKGREY;
extern uint32_t COL_MIDGREY, COL_HIGHLIGHT;
extern uint32_t COL_TOOLBAR_BG, COL_BTN_ACTIVE;
