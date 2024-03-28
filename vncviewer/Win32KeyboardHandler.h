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

  bool handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;
  bool handleKeyRelease(int keyCode) override;

public slots:
  void pushLEDState() override;
  void setLEDState(unsigned int state) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

private:
  bool altGrArmed_ = false;
  unsigned int altGrCtrlTime_;
  QTimer altGrCtrlTimer_;

  void resolveAltGrDetection(bool isAltGrSequence);
  bool handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
  bool handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // WIN32KEYBOARDHANDLER_H
