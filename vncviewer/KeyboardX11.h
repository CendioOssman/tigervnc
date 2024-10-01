#ifndef X11KEYBOARDHANDLER_H
#define X11KEYBOARDHANDLER_H

#include "Keyboard.h"

struct _XDisplay;

class KeyboardX11 : public Keyboard
{
  Q_OBJECT

public:
  KeyboardX11(QObject* parent = nullptr);
  ~KeyboardX11();

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

signals:
  void message(QString const& msg, int timeout);

private:
  _XDisplay* display;
  int eventNumber;
  QTimer keyboardGrabberTimer;

  unsigned int getModifierMask(unsigned int keysym);
};

#endif // X11KEYBOARDHANDLER_H
