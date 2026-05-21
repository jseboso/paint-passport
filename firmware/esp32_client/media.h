#pragma once
#include "config.h"
#include <SD.h>
#include <SPI.h>

// Defined in paint_app.ino
void drawCloseButton();
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

extern uint8_t  paintingCount;
extern int16_t  galleryScrollX;
extern bool     mediaChromeDrawn;
