#pragma once
#include "config.h"
#include <SD.h>
#include <SPI.h>

// Defined in esp32_client.ino
void drawCloseButton(uint16_t x, uint16_t y);
void drawPanelBase(const char* title);
void closePanel();

bool  initSD();
bool  sdAvailable();
bool  savePainting();
void  loadPainting(uint8_t index);
void  scanPaintings();
void  drawMediaPanel();
void  handleMediaTouch(int16_t x, int16_t y);
void  handleMediaRelease(int16_t x, int16_t y);

// reads a saved-painting BMP straight into a sprite
bool  loadBMPToSprite(const char* path, LGFX_Sprite& sprite);

extern uint8_t  paintingCount;
extern int16_t  galleryScrollX;
extern bool     mediaChromeDrawn;
