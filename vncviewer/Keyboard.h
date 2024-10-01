#ifndef BASEKEYBOARDHANDLER_H
#define BASEKEYBOARDHANDLER_H

#include <QAbstractNativeEventFilter>
#include <QDataStream>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"

class KeyboardHandler {
public:
  virtual void handleKeyPress(int systemKeyCode,
                              uint32_t keyCode, uint32_t keySym) = 0;
  virtual void handleKeyRelease(int systemKeyCode) = 0;
};

class Keyboard
{
public:
  Keyboard(KeyboardHandler* handler);

  virtual bool handleEvent(const char* eventType, void* message) = 0;

  virtual void reset() {};

  virtual void grabKeyboard();
  virtual void ungrabKeyboard();

  virtual unsigned getLEDState() = 0;
  virtual void setLEDState(unsigned state) = 0;

  void setMenuKeyStatus(quint32 keysym, bool checked);

  bool getMenuCtrlKey() const;
  bool getMenuAltKey() const;

  void setContextMenuVisible(bool newContextMenuVisible);

  bool isKeyboardGrabbed() const;

protected:
  KeyboardHandler* handler;

  bool keyboardGrabbed = false;

  bool menuCtrlKey = false;
  bool menuAltKey = false;
};

#endif // BASEKEYBOARDHANDLER_H
