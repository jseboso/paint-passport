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

// Hit-tests the Created/Received tabs in the Media panel's title strip.
// Returns true (and switches the visible gallery) if (x,y) hit one.
bool  handleMediaTabTouch(int16_t x, int16_t y);

// reads a saved-painting BMP straight into a sprite
bool  loadBMPToSprite(const char* path, LGFX_Sprite& sprite);

// Receiving a painting sent from the web gallery (see cloud.cpp). Two-step so
// the HTTP download can be streamed straight to SD without buffering the
// whole image in RAM
bool  beginInboxReceive(char* outPath, size_t outPathLen);
bool  finishInboxReceive();

// Uploads the gallery entry at this paintingNames[] index (as returned by
// buildVisibleIndices()/handleMediaRelease()
bool  uploadPaintingAtIndex(uint8_t index);

extern uint8_t  paintingCount;
extern int16_t  galleryScrollX;
extern bool     mediaChromeDrawn;
