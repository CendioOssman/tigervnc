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

using DownMap = std::map<int, quint32>;

class BaseKeyboardHandler : public QObject, public QAbstractNativeEventFilter
{
  Q_OBJECT

public:
  BaseKeyboardHandler(QObject* parent = nullptr);

  virtual void grabKeyboard();
  virtual void ungrabKeyboard();

  virtual void setLEDState(unsigned int state) = 0;
  virtual void pushLEDState() = 0;

  virtual bool handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false);
  virtual bool handleKeyRelease(int keyCode);
  void resetKeyboard();

  void setMenuKeyStatus(quint32 keysym, bool checked);

  bool getMenuCtrlKey() const;
  bool getMenuAltKey() const;

  void setContextMenuVisible(bool newContextMenuVisible);

signals:
  void contextMenuKeyPressed(bool menuShortCutMode);

protected:
  bool keyboardGrabbed = false;

  DownMap downKeySym;

  quint32 menuKeySym = XK_F8;
  bool menuCtrlKey = false;
  bool menuAltKey = false;
};

#endif // BASEKEYBOARDHANDLER_H
