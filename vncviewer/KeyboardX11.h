#ifndef X11KEYBOARDHANDLER_H
#define X11KEYBOARDHANDLER_H

#include "Keyboard.h"

struct _XDisplay;

class KeyboardX11 : public Keyboard
{
  Q_OBJECT

public:
  KeyboardX11(KeyboardHandler* handler, QObject* parent);
  ~KeyboardX11();

  bool handleEvent(const char* eventType, void* message) override;

public slots:
  unsigned getLEDState() override;
  void setLEDState(unsigned state) override;
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
