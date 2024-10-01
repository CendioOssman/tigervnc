#ifndef WIN32KEYBOARDHANDLER_H
#define WIN32KEYBOARDHANDLER_H

#include "BaseKeyboardHandler.h"

#include <QTimer>
#include <windows.h>

class KeyboardWin32 : public BaseKeyboardHandler
{
  Q_OBJECT

public:
  KeyboardWin32(QObject* parent = nullptr);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  bool nativeEventFilter(QByteArray const& eventType, void* message, long* result) override;
#else
  bool nativeEventFilter(QByteArray const& eventType, void* message, qintptr* result) override;
#endif

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
