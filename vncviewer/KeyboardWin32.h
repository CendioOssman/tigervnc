#ifndef WIN32KEYBOARDHANDLER_H
#define WIN32KEYBOARDHANDLER_H

#include <rfb/Timer.h>

#include "Keyboard.h"

#include <windows.h>

class KeyboardWin32 : public Keyboard
{
public:
  KeyboardWin32(KeyboardHandler* handler);

  bool handleEvent(const char* eventType, void* message) override;

  unsigned getLEDState() override;
  void setLEDState(unsigned state) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void handleAltGrTimeout(rfb::Timer*);

private:
  bool leftShiftDown, rightShiftDown;

  bool altGrArmed = false;
  unsigned int altGrCtrlTime;
  rfb::MethodTimer<KeyboardWin32> altGrCtrlTimer;

  void resolveAltGrDetection(bool isAltGrSequence);
  bool handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
  bool handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // WIN32KEYBOARDHANDLER_H
