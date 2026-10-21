#include "Pixel.h"
#include "TFTs.h"

// TFT_eSPI.h always includes the Adafruit 5x7 font as `font` (5 column bytes per character, bit 0 = top row),
// even with LOAD_GLCD off; we draw it ourselves as blocks.

namespace Pixel
{
  uint16_t color(uint32_t rgb)
  {
    uint16_t d = tfts.dimming;
    return tfts.color565(((rgb >> 16) & 0xFF) * d / 255, ((rgb >> 8) & 0xFF) * d / 255, (rgb & 0xFF) * d / 255);
  }

  void begin(uint8_t tube)
  {
    tfts.selectCustom(tube);
  }

  void clear()
  {
    tfts.fillScreen(TFT_BLACK);
  }

  int16_t textWidth(const char *s, uint8_t scale)
  {
    size_t n = strlen(s);
    return n == 0 ? 0 : int16_t(n * 6 - 1) * scale;
  }

  uint8_t fitScale(const char *s, uint8_t max_scale)
  {
    uint8_t scale = max_scale;
    while (scale > 1 && textWidth(s, scale) > WIDTH)
      scale--;
    return scale;
  }

  void text(int16_t x, int16_t y, const char *s, uint8_t scale, uint32_t fg)
  {
    uint16_t c = color(fg);
    int16_t block = scale >= 4 ? scale - 1 : scale; // Gap between blocks gives the pixel-matrix look
    tfts.fillRect(x, y, textWidth(s, scale), 8 * scale, TFT_BLACK);
    for (; *s; s++, x += 6 * scale)
    {
      uint8_t ch = uint8_t(*s);
      for (uint8_t col = 0; col < 5; col++)
      {
        uint8_t bits = font[ch * 5 + col];
        for (uint8_t row = 0; row < 8; row++) // Row 7: descenders (g, p, y)
          if (bits & (1 << row))
            tfts.fillRect(x + col * scale, y + row * scale, block, block, c);
      }
    }
  }

  void textCentered(int16_t y, const char *s, uint8_t scale, uint32_t fg)
  {
    tfts.fillRect(0, y, WIDTH, 8 * scale, TFT_BLACK);
    text((WIDTH - textWidth(s, scale)) / 2, y, s, scale, fg);
  }

  int16_t textWrapped(int16_t x, int16_t y, int16_t w, const char *s, uint8_t scale, uint32_t fg, uint8_t max_lines,
                      bool center)
  {
    const size_t per_line = (w + scale) / (6 * scale);
    char line[48];
    const char *p = s;
    for (uint8_t n = 0; n < max_lines && *p; n++)
    {
      while (*p == ' ')
        p++;
      size_t len = strnlen(p, per_line + 1);
      if (len > per_line)
      { // Break at the last space that fits, or hard-break a long word
        len = per_line;
        for (size_t i = per_line; i > 0; i--)
          if (p[i] == ' ')
          {
            len = i;
            break;
          }
      }
      len = min(len, sizeof(line) - 1);
      memcpy(line, p, len);
      line[len] = 0;
      p += len;
      bool more = *p != 0 && n == max_lines - 1;
      if (more) // Cut: mark it
      {
        size_t keep = min(len, per_line - 2);
        line[keep] = line[keep + 1] = '.';
        line[keep + 2] = 0;
      }
      int16_t lx = center ? x + (w - textWidth(line, scale)) / 2 : x;
      text(lx, y, line, scale, fg);
      y += 9 * scale;
    }
    return y;
  }

  void art(int16_t x, int16_t y, uint8_t cell, const char *const *rows, uint8_t row_count, const Ink *inks,
           uint8_t ink_count)
  {
    for (uint8_t r = 0; r < row_count; r++)
      for (uint8_t c = 0; rows[r][c]; c++)
      {
        uint32_t rgb = BLACK;
        for (uint8_t i = 0; i < ink_count; i++)
          if (inks[i].key == rows[r][c])
            rgb = inks[i].color;
        tfts.fillRect(x + c * cell, y + r * cell, cell, cell, rgb == BLACK ? TFT_BLACK : color(rgb));
      }
  }

  void gauge(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t segments, float fraction, uint32_t fg, bool vertical)
  {
    uint16_t on = color(fg), off = color(UNLIT);
    float lit = constrain(fraction, 0.0f, 1.0f) * segments;
    int16_t length = vertical ? h : w;
    int16_t gap = 3, seg = (length - gap * (segments - 1)) / segments;
    for (uint8_t i = 0; i < segments; i++)
    {
      // A partly reached segment counts from half way, so 5 % shows nothing and 95 % shows all.
      uint16_t c = lit >= i + 0.5f ? on : off;
      int16_t offset = i * (seg + gap);
      if (vertical)
        tfts.fillRect(x, y + h - offset - seg, w, seg, c);
      else
        tfts.fillRect(x + offset, y, seg, h, c);
    }
  }

  // Spinner glyphs: arms from the center of an 11x11 grid.
  struct Glyph
  {
    uint8_t arms;
    float length;
    float rotation_deg;
  };
  static const Glyph GLYPHS[Spinner::FRAMES] = {
      {0, 0, 0},     // ·
      {4, 3, 0},     // ✢
      {8, 3, 0},     // ✳
      {6, 4.2f, 90}, // ✶
      {8, 4.2f, 0},  // ✻
      {8, 5.2f, 22.5f}, // ✽
  };

  static void rasterize(uint8_t frame, uint8_t grid[11][11])
  {
    memset(grid, 0, 11 * 11);
    const Glyph &g = GLYPHS[frame % Spinner::FRAMES];
    grid[5][5] = 1;
    if (g.arms == 0)
    { // The dot: a small round blob
      grid[4][5] = grid[6][5] = grid[5][4] = grid[5][6] = 1;
      return;
    }
    for (uint8_t a = 0; a < g.arms; a++)
    {
      float angle = (g.rotation_deg + 360.0f * a / g.arms) * float(M_PI) / 180.0f;
      for (float t = 0.5f; t <= g.length; t += 0.25f)
      {
        int8_t gx = int8_t(lroundf(5 + t * cosf(angle))), gy = int8_t(lroundf(5 + t * sinf(angle)));
        if (gx >= 0 && gx < 11 && gy >= 0 && gy < 11)
          grid[gy][gx] = 1;
      }
    }
  }

  void Spinner::draw(int16_t y, uint8_t frame, uint32_t fg, bool force, uint8_t cell)
  {
    uint8_t grid[11][11];
    rasterize(frame, grid);
    force = force || fg != shown_color;
    uint16_t on = color(fg);
    const int16_t x0 = (WIDTH - 11 * cell) / 2;
    for (uint8_t gy = 0; gy < 11; gy++)
      for (uint8_t gx = 0; gx < 11; gx++)
        if (force || grid[gy][gx] != shown[gy][gx])
          tfts.fillRect(x0 + gx * cell, y + gy * cell, cell - 1, cell - 1, grid[gy][gx] ? on : TFT_BLACK);
    memcpy(shown, grid, sizeof(shown));
    shown_color = fg;
  }

  uint8_t Spinner::frameAt(uint32_t ms, uint16_t period_ms)
  {
    uint8_t step = (ms / period_ms) % (2 * FRAMES - 2); // 0 1 2 3 4 5 4 3 2 1
    return step < FRAMES ? step : 2 * FRAMES - 2 - step;
  }
} // namespace Pixel
