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

public slots:
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;

signals:
  void message(QString const& msg, int timeout);

private:
  _XDisplay* display;
  int eventNumber;
  QTimer keyboardGrabberTimer;

  unsigned int getModifierMask(unsigned int keysym);
};

#endif // X11KEYBOARDHANDLER_H
