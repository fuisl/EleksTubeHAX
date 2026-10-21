#include "ChipSelect.h"

void ChipSelect::begin()
{
  pinMode(CSSR_LATCH_PIN, OUTPUT);
  pinMode(CSSR_DATA_PIN, OUTPUT);
  pinMode(CSSR_CLOCK_PIN, OUTPUT);

  digitalWrite(CSSR_DATA_PIN, LOW);
  digitalWrite(CSSR_CLOCK_PIN, LOW);
  digitalWrite(CSSR_LATCH_PIN, LOW);
  update();
}

void ChipSelect::clear(bool update_)
{
  setDigitMap(all_off, update_);
}

void ChipSelect::setAll(bool update_)
{
  setDigitMap(all_on, update_);
}

void ChipSelect::setDigit(uint8_t digit, bool update_)
{
  // Set the bit for the given digit in the digits_map
  setDigitMap(1 << digit, update_);
  if (update_)
    update();
}

void ChipSelect::update()
{
  // Documented in README.md.  Q7 and Q6 are unused. Q5 is Seconds Ones, Q0 is Hours Tens.
  // Q7 is the first bit written, Q0 is the last.  So we push two dummy bits, then start with
  // Seconds Ones and end with Hours Tens.
  // CS is Active Low, but digits_map is 1 for enable, 0 for disable.  So we bit-wise NOT first.

  uint8_t to_shift = (~digits_map) << 2;

  digitalWrite(CSSR_LATCH_PIN, LOW);
  shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, LSBFIRST, to_shift);
  digitalWrite(CSSR_LATCH_PIN, HIGH);
}

bool ChipSelect::isSecondsOnes()
{
  return ((digits_map & SECONDS_ONES_MAP) > 0);
}

bool ChipSelect::isSecondsTens()
{
  return ((digits_map & SECONDS_TENS_MAP) > 0);
}

bool ChipSelect::isMinutesOnes()
{
  return ((digits_map & MINUTES_ONES_MAP) > 0);
}

bool ChipSelect::isMinutesTens()
{
  return ((digits_map & MINUTES_TENS_MAP) > 0);
}

bool ChipSelect::isHoursOnes()
{
  return ((digits_map & HOURS_ONES_MAP) > 0);
}

bool ChipSelect::isHoursTens()
{
  return ((digits_map & HOURS_TENS_MAP) > 0);
}

void ChipSelect::setEnabled(bool enabled)
{
  digitalWrite(TFT_ENABLE_PIN, enabled ? ACTIVATEDISPLAYS : DEACTIVATEDISPLAYS);
}

