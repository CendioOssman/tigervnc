#include "BaseKeyboardHandler.h"

#include "appmanager.h"
#include "parameters.h"

#include <QDebug>

BaseKeyboardHandler::BaseKeyboardHandler(QObject* parent)
  : QObject(parent)
{
}

void BaseKeyboardHandler::grabKeyboard()
{
  keyboardGrabbed = true;
}

void BaseKeyboardHandler::ungrabKeyboard()
{
  keyboardGrabbed = true;
}

void BaseKeyboardHandler::setMenuKeyStatus(quint32 keysym, bool checked)
{
  if (keysym == XK_Control_L) {
    menuCtrlKey = checked;
  } else if (keysym == XK_Alt_L) {
    menuAltKey = checked;
  }
}

bool BaseKeyboardHandler::getMenuCtrlKey() const
{
  return menuCtrlKey;
}

bool BaseKeyboardHandler::getMenuAltKey() const
{
  return menuAltKey;
}

void BaseKeyboardHandler::resetKeyboard()
{
  while (!downKeySym.empty()) {
    handleKeyRelease(downKeySym.begin()->first);
  }
}
