#ifndef WIN32KEYBOARDHANDLER_H
#define WIN32KEYBOARDHANDLER_H

#include "Keyboard.h"

#include <QTimer>
#include <windows.h>

class KeyboardWin32 : public Keyboard
{
  Q_OBJECT

public:
  KeyboardWin32(KeyboardHandler* handler, QObject* parent);

  bool handleEvent(const char* eventType, void* message) override;

public slots:
  unsigned getLEDState() override;
  void setLEDState(unsigned state) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

private:
  bool leftShiftDown, rightShiftDown;

  bool altGrArmed = false;
  unsigned int altGrCtrlTime;
  QTimer altGrCtrlTimer;

  void resolveAltGrDetection(bool isAltGrSequence);
  bool handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
  bool handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // WIN32KEYBOARDHANDLER_H
