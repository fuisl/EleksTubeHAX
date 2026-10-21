#ifndef FAST_DIGITS_H
#define FAST_DIGITS_H

/*
 * RAM cache of one clock face at half resolution, for digits that change many times per second
 * (the countdown's hundredths).
 *
 * Normal digits are decoded from LittleFS on every change: ~41 ms load + ~14 ms push (measured, issue #10).
 * Here all 10 digits stay in RAM as 8-bit palette indices (68x120 + RGB565 palette, ~8.7 KB each,
 * ~87 KB total) and are pixel-doubled while pushing, so a digit change is a ~14 ms push only.
 * Only 8-bit palettized 135x240 BMPs are supported (that is what data/ ships).
 */
#include <Arduino.h>
#include "GLOBAL_DEFINES.h"

class FastDigits
{
public:
  // Decode the 10 digits of the clock face, unless they are already cached for this face and dimming.
  // Returns false if the face can't be decoded or there is not enough memory (callers fall back to TFTs::setDigit).
  bool ensure(uint8_t graphic, uint8_t dimming);
  void release();
  bool isLoaded() { return digits[0] != nullptr; }

  // Draw value (0..9) on the given display (SECONDS_ONES...HOURS_TENS).
  void draw(uint8_t position, uint8_t value);

private:
  static const uint16_t W = (TFT_WIDTH + 1) / 2;
  static const uint16_t H = TFT_HEIGHT / 2;
  struct Digit
  {
    uint16_t palette[256]; // RGB565, dimming applied
    uint8_t pixels[W * H];
  };
  Digit *digits[10] = {};
  uint8_t loaded_graphic = 0;
  uint8_t loaded_dimming = 0;

  bool decode(uint8_t file_index, uint8_t dimming, Digit &out);
};

#endif // FAST_DIGITS_H
