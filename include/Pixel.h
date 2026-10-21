#ifndef PIXEL_H
#define PIXEL_H

/*
 * Claude CLI pixel theme for the non-time tubes (docs/DESIGN.md#visual-language): orange on black,
 * a chunky 5x7 pixel font drawn as blocks, the CLI spinner glyphs, segmented gauges.
 * Everything is dimmed together with the nixie images at night.
 *
 * Usage: Pixel::begin(tube) selects one display (and makes its next setDigit() redraw), then draw.
 */
#include <Arduino.h>

namespace Pixel
{
  // Claude CLI palette (RGB888)
  const uint32_t ORANGE = 0xD97757;
  const uint32_t AMBER = 0xE8A33D;
  const uint32_t GREEN = 0x7FB069;
  const uint32_t ROSE = 0xC96A5B;
  const uint32_t TEXT = 0xD8D3CC;
  const uint32_t MUTED = 0x8F8A83;
  const uint32_t UNLIT = 0x3A2620; // Unlit gauge segments: very dark orange
  const uint32_t BLACK = 0x000000;

  const int16_t WIDTH = 135, HEIGHT = 240;

  uint16_t color(uint32_t rgb); // RGB565 with the current night dimming
  void begin(uint8_t tube);
  void clear();

  int16_t textWidth(const char *s, uint8_t scale);
  // Draws s with each font pixel as a block (with a 1 px gap from scale 4 up). Clears its own box first.
  void text(int16_t x, int16_t y, const char *s, uint8_t scale, uint32_t fg);
  // Centered horizontally; clears the whole band (full width, 8 * scale high) first.
  void textCentered(int16_t y, const char *s, uint8_t scale, uint32_t fg);
  // Largest scale (up to max_scale) at which s fits the width.
  uint8_t fitScale(const char *s, uint8_t max_scale);

  // Segmented gauge, filled left to right (horizontal) or bottom to top (vertical).
  void gauge(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t segments, float fraction, uint32_t fg, bool vertical);

  // The Claude CLI spinner (· ✢ ✳ ✶ ✻ ✽) on an 11x11 grid of 11 px blocks; only changed blocks are redrawn.
  class Spinner
  {
  public:
    static const uint8_t FRAMES = 6;
    void draw(int16_t y, uint8_t frame, uint32_t fg, bool force);
    // Ping-pong sequence 0..5..1 like the CLI, one step per period_ms.
    static uint8_t frameAt(uint32_t ms, uint16_t period_ms = 120);

  private:
    uint8_t shown[11][11] = {};
    uint32_t shown_color = 0xFFFFFFFF;
  };
} // namespace Pixel

#endif // PIXEL_H
