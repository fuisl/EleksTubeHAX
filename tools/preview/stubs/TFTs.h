// Desktop TFTs: six 135x240 RGB565 framebuffers instead of the SPI displays.
#pragma once
#include <Arduino.h>
#include "glcdfont.c"
#define TFT_BLACK 0x0000
#define TFT_WIDTH 135
#define TFT_HEIGHT 240

class TFT_eSPI
{
public:
  virtual ~TFT_eSPI() {}
  virtual void fillRect(int x, int y, int w, int h, uint16_t c) = 0;
  void fillScreen(uint16_t c) { fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, c); }
};

// No off-screen frames on the desktop: createSprite fails, so Pixel draws directly.
class TFT_eSprite : public TFT_eSPI
{
public:
  TFT_eSprite(TFT_eSPI *) {}
  void setColorDepth(int) {}
  void *createSprite(int, int) { return nullptr; }
  void createPalette(uint16_t *, int) {}
  void setPaletteColor(uint8_t, uint16_t) {}
  void fillSprite(uint16_t) {}
  void pushSprite(int, int) {}
  void deleteSprite() {}
  void fillRect(int, int, int, int, uint16_t) override {}
};

class TFTs : public TFT_eSPI
{
public:
  static const uint8_t blanked = 255, custom = 254;
  uint8_t dimming = 255;
  bool show_status_overlays = true;
  uint16_t fb[NUM_DIGITS][TFT_HEIGHT][TFT_WIDTH] = {};
  uint8_t current = 0, digits[NUM_DIGITS] = {};
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
  void selectCustom(uint8_t d) { current = d, digits[d] = custom; }
  uint8_t getDigit(uint8_t d) { return digits[d]; }
  void fillRect(int x, int y, int w, int h, uint16_t c) override
  {
    for (int yy = std::max(y, 0); yy < std::min(y + h, TFT_HEIGHT); yy++)
      for (int xx = std::max(x, 0); xx < std::min(x + w, TFT_WIDTH); xx++)
        fb[current][yy][xx] = c;
  }
};
extern TFTs tfts;
