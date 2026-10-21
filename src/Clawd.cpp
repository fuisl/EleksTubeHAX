#include "Clawd.h"
#include "Pixel.h"
#include "TFTs.h"

namespace
{
  // Cell colors
  enum Ink : uint8_t
  {
    none,
    body,
    keys,  // Keyboard
    sweat,
    zzz
  };
  const uint32_t INK_RGB[] = {Pixel::BLACK, Pixel::ORANGE, Pixel::ORANGE_DIM, 0x7FC8F8, Pixel::MUTED};

  struct Grid
  {
    uint8_t (*g)[15];
    void set(int8_t row, int8_t col, uint8_t ink)
    {
      if (row >= 0 && row < 16 && col >= 0 && col < 15)
        g[row][col] = ink;
    }
  };
} // namespace

void Clawd::start(Action a, uint32_t ms, uint32_t length)
{
  action = a;
  action_start = ms;
  action_length = length;
}

void Clawd::poke()
{
  start(hop, millis(), 900);
}

void Clawd::lookRight()
{
  start(look_right, millis(), 1600);
}

void Clawd::compose(uint8_t g[ROWS][COLS], uint32_t ms, Mood mood)
{
  memset(g, none, ROWS * COLS);
  Grid grid{g};
  uint32_t t = ms - action_start;

  // Posture for the current action
  int8_t lift = 0;                  // Body moved up by this many cells
  int8_t eye_dx = 0, eye_dy = 0;    // Gaze
  bool eyes_closed = mood == sleepy;
  enum Arm : uint8_t { side, up, low } left = side, right = side;
  bool keyboard = false;

  switch (action)
  {
  case blink:
    eyes_closed = true;
    break;
  case look_left:
    eye_dx = -1;
    break;
  case look_right: // Glance at the tube to the right, then type about it
    eye_dx = 1;
    if (t > 600)
    {
      eye_dx = 0, eye_dy = 1, keyboard = true;
      (((t / 160) % 2) ? left : right) = low;
    }
    break;
  case wave:
    right = (t / 280) % 2 ? up : side;
    break;
  case type:
    eye_dy = 1, keyboard = true;
    (((t / 160) % 2) ? left : right) = low;
    break;
  case hop:
    lift = t < 450 ? (t < 120 || t > 330 ? 1 : 2) : 0;
    right = t > 300 ? ((t / 200) % 2 ? up : side) : up;
    left = t < 450 ? up : side;
    break;
  case panic:
    left = right = up;
    eye_dx = (t / 250) % 2 ? 1 : -1;
    break;
  default:
    break;
  }

  // Body: 9 x 7 block, eyes in the upper half
  for (int8_t r = 5; r <= 11; r++)
    for (int8_t c = 3; c <= 11; c++)
      grid.set(r - lift, c, body);
  for (int8_t c : {5, 9})
  {
    if (eyes_closed)
      grid.set(8 - lift, c + eye_dx, none), grid.set(8 - lift, c + eye_dx + (c == 5 ? 1 : -1), none);
    else
      grid.set(7 + eye_dy - lift, c + eye_dx, none), grid.set(8 + eye_dy - lift, c + eye_dx, none);
  }
  // Arms: one-cell stubs at the sides; raised they go up along the body, lowered one row down
  auto arm = [&](Arm a, int8_t inner, int8_t outer)
  {
    if (a == side)
      grid.set(9 - lift, inner, body);
    else if (a == up)
      grid.set(8 - lift, inner, body), grid.set(7 - lift, outer, body), grid.set(6 - lift, outer, body);
    else
      grid.set(10 - lift, inner, body);
  };
  arm(left, 2, 1);
  arm(right, 12, 13);
  // Legs: four stubs, tucked in mid-hop
  for (int8_t c : {4, 6, 8, 10})
  {
    grid.set(12 - lift, c, body);
    if (lift == 0)
      grid.set(13, c, body);
  }
  if (keyboard)
    for (int8_t c = 1; c <= 13; c++)
      grid.set(14, c, (c % 3) ? keys : none);

  if (mood == worried)
    grid.set(3 - lift, 12, sweat), grid.set(4 - lift, 12, sweat), grid.set(4 - lift, 13, sweat);
  if (mood == sleepy)
  { // A big and a small z rising from the head in turn
    static const char *const BIG[] = {"####", "..#.", ".#..", "####"};
    static const char *const SMALL[] = {"###", ".#.", "#..", "###"};
    int8_t big_row = 4 - int8_t((ms / 600) % 6), small_row = 3 - int8_t((ms / 600 + 3) % 6);
    for (int8_t r = 0; r < 4; r++)
      for (int8_t c = 0; c < 4; c++)
        if (BIG[r][c] == '#' && big_row + r < 5)
          grid.set(big_row + r - 1, 10 + c, zzz);
    for (int8_t r = 0; r < 4; r++)
      for (int8_t c = 0; c < 3; c++)
        if (SMALL[r][c] == '#' && small_row + r < 4)
          grid.set(small_row + r - 1, 6 + c, zzz);
  }
}

void Clawd::update(uint32_t ms, Mood mood)
{
  if (action != idle && ms - action_start >= action_length)
    action = idle;
  if (action == idle && int32_t(ms - next_action) >= 0)
  {
    // Next idle behavior: mostly calm, a little busier when worried, nothing when asleep.
    uint8_t roll = random(100);
    if (mood == sleepy)
      ;
    else if (mood == worried && roll < 35)
      start(roll < 15 ? panic : (roll % 2 ? look_left : look_right), ms, 1200);
    else if (roll < 35)
      start(blink, ms, 160);
    else if (roll < 55)
      start(roll % 2 ? look_left : look_right, ms, 1400);
    else if (roll < 72)
      start(type, ms, 2400);
    else if (roll < 86)
      start(wave, ms, 1700);
    else
      start(hop, ms, 900);
    next_action = ms + action_length + random(1800, 4500);
  }

  uint8_t grid[ROWS][COLS];
  compose(grid, ms, mood);
  const int16_t x0 = (Pixel::WIDTH - COLS * CELL) / 2;
  for (uint8_t r = 0; r < ROWS; r++)
    for (uint8_t c = 0; c < COLS; c++)
      if (force || grid[r][c] != shown[r][c])
        tfts.fillRect(x0 + c * CELL, TOP + r * CELL, CELL, CELL,
                      grid[r][c] == none ? TFT_BLACK : Pixel::color(INK_RGB[grid[r][c]]));
  memcpy(shown, grid, sizeof(shown));
  force = false;
}
