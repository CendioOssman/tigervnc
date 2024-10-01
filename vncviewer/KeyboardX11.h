#ifndef X11KEYBOARDHANDLER_H
#define X11KEYBOARDHANDLER_H

#include <rfb/Timer.h>

#include "Keyboard.h"

struct _XDisplay;

class KeyboardX11 : public Keyboard
{
public:
  KeyboardX11(KeyboardHandler* handler);
  ~KeyboardX11();

  bool handleEvent(const char* eventType, void* message) override;

public slots:
  unsigned getLEDState() override;
  void setLEDState(unsigned state) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void retryGrab(rfb::Timer*);

signals:
  void message(QString const& msg, int timeout);

private:
  _XDisplay* display;
  int eventNumber;
  rfb::MethodTimer<KeyboardX11> keyboardGrabberTimer;

  unsigned int getModifierMask(unsigned int keysym);
};

#endif // X11KEYBOARDHANDLER_H
