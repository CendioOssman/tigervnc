#ifndef WIN32KEYBOARDHANDLER_H
#define WIN32KEYBOARDHANDLER_H

#include "BaseKeyboardHandler.h"

#include <QTimer>
#include <windows.h>

class Win32KeyboardHandler : public BaseKeyboardHandler
{
  Q_OBJECT

public:
  Win32KeyboardHandler(QObject* parent = nullptr);

  bool nativeEventFilter(QByteArray const& eventType, void* message, long*) override;

public slots:
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

private:
  bool altGrArmed = false;
  unsigned int altGrCtrlTime;
  QTimer altGrCtrlTimer;

  void resolveAltGrDetection(bool isAltGrSequence);
  bool handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
  bool handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // WIN32KEYBOARDHANDLER_H
