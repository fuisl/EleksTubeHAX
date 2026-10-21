#include "FastDigits.h"
#include <LittleFS.h>
#include "TFTs.h"

static uint32_t readLE(fs::File &f, uint8_t bytes)
{
  uint32_t v = 0;
  for (uint8_t i = 0; i < bytes; i++)
    v |= uint32_t(f.read()) << (8 * i);
  return v;
}

bool FastDigits::decode(uint8_t file_index, uint8_t dimming, Digit &out)
{
  char filename[10];
  snprintf(filename, sizeof(filename), "/%d.bmp", file_index);
  fs::File f = LittleFS.open(filename, "r");
  if (!f)
    return false;

  bool ok = false;
  if (readLE(f, 2) == 0x4D42) // "BM"
  {
    f.seek(10);
    uint32_t data_offset = readLE(f, 4);
    uint32_t header_size = readLE(f, 4);
    int32_t w = int32_t(readLE(f, 4));
    int32_t h = int32_t(readLE(f, 4));
    readLE(f, 2); // planes
    uint16_t bpp = readLE(f, 2);
    uint32_t compression = readLE(f, 4);
    f.seek(46);
    uint32_t colors = readLE(f, 4);
    if (colors == 0 || colors > 256)
      colors = 256;

    if (bpp == 8 && compression == 0 && w == TFT_WIDTH && h == TFT_HEIGHT)
    {
      f.seek(14 + header_size);
      for (uint16_t i = 0; i < colors; i++)
      {
        uint32_t c = readLE(f, 4); // B, G, R, reserved
        uint16_t color = ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c & 0xFF) >> 3);
        out.palette[i] = dimming < 255 ? tfts.alphaBlend(dimming, color, TFT_BLACK) : color;
      }

      // Rows are stored bottom-up and padded to 4 bytes. Keep every other row and column.
      const uint32_t row_size = (TFT_WIDTH + 3) & ~3u;
      uint8_t row[row_size];
      f.seek(data_offset);
      for (int32_t file_row = 0; file_row < h; file_row++)
      {
        f.read(row, row_size);
        int32_t y = h - 1 - file_row;
        if (y & 1)
          continue;
        uint8_t *dst = out.pixels + (y / 2) * W;
        for (uint16_t x = 0; x < W; x++)
          dst[x] = row[x * 2];
      }
      ok = true;
    }
  }
  f.close();
  return ok;
}

bool FastDigits::ensure(uint8_t graphic, uint8_t dimming)
{
  if (isLoaded() && loaded_graphic == graphic && loaded_dimming == dimming)
    return true;

  uint32_t start = millis();
  for (uint8_t d = 0; d < 10; d++)
  {
    if (digits[d] == nullptr)
      digits[d] = static_cast<Digit *>(malloc(sizeof(Digit)));
    if (digits[d] == nullptr || !decode(graphic * 10 + d, dimming, *digits[d]))
    {
      Serial.printf("FastDigits: can't cache digit %u (free heap %u)\n", d, ESP.getFreeHeap());
      release();
      return false;
    }
  }
  loaded_graphic = graphic;
  loaded_dimming = dimming;
  Serial.printf("FastDigits: cached face %u in %lu ms, free heap %u\n", graphic, millis() - start, ESP.getFreeHeap());
  return true;
}

void FastDigits::release()
{
  if (!isLoaded())
    return;
  for (uint8_t d = 0; d < 10; d++)
  {
    free(digits[d]);
    digits[d] = nullptr;
  }
  Serial.printf("FastDigits: released, free heap %u\n", ESP.getFreeHeap());
}

void FastDigits::draw(uint8_t position, uint8_t value)
{
  if (!isLoaded() || value > 9 || !tfts.isEnabled())
    return;
  const Digit &digit = *digits[value];
  uint16_t line[TFT_WIDTH];

  tfts.chip_select.setDigit(position);
  bool old_swap = tfts.getSwapBytes();
  tfts.setSwapBytes(true);
  tfts.startWrite();
  tfts.setAddrWindow(0, 0, TFT_WIDTH, TFT_HEIGHT);
  for (uint16_t y = 0; y < H; y++)
  {
    const uint8_t *src = digit.pixels + y * W;
    for (uint16_t x = 0; x < TFT_WIDTH; x++)
      line[x] = digit.palette[src[x >> 1]];
    tfts.pushPixels(line, TFT_WIDTH); // Each source row is shown twice
    tfts.pushPixels(line, TFT_WIDTH);
  }
  tfts.endWrite();
  tfts.setSwapBytes(old_swap);
}
