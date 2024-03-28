#ifndef X11KEYBOARDHANDLER_H
#define X11KEYBOARDHANDLER_H

#include "BaseKeyboardHandler.h"

struct _XDisplay;

class X11KeyboardHandler : public BaseKeyboardHandler
{
  Q_OBJECT

public:
  X11KeyboardHandler(QObject* parent = nullptr);
  ~X11KeyboardHandler();

  bool nativeEventFilter(QByteArray const& eventType, void* message, long*) override;

  bool handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;
  bool handleKeyRelease(int keyCode) override;
  void releaseKeyboard();

public slots:
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

signals:
  void message(QString const& msg, int timeout);

private:
  _XDisplay* display_;
  int eventNumber_;
  QTimer keyboardGrabberTimer_;

  unsigned int getModifierMask(unsigned int keysym);
};

#endif // X11KEYBOARDHANDLER_H
