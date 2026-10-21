#pragma once
#include <Arduino.h>
enum class ButtonId : uint8_t { Left, Mode, Right, Power };
enum class Gesture : uint8_t { Tap, Hold, LongHold };
struct ButtonEvent { ButtonId button; Gesture gesture; };
class App
{
public:
  virtual ~App() {}
  virtual const char *name() const = 0;
  virtual void enter() = 0;
  virtual void exit() {}
  virtual void loop() = 0;
  virtual void background() {}
  virtual bool onButton(const ButtonEvent &) { return false; }
};
