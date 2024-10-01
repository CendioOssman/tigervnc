#include "Keyboard.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "appmanager.h"
#include "i18n.h"
#include "menukey.h"
#include "parameters.h"
#include "rdr/Exception.h"
#include "rfb/LogWriter.h"
#include "vncconnection.h"

#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/XKBlib.h>
#endif

static rfb::LogWriter vlog("Keyboard");

Keyboard::Keyboard(KeyboardHandler* handler_, QObject* parent)
  : QObject(parent), handler(handler_)
{
}

void Keyboard::grabKeyboard()
{
  keyboardGrabbed = true;
}

void Keyboard::ungrabKeyboard()
{
  keyboardGrabbed = false;
}

void Keyboard::setMenuKeyStatus(quint32 keysym, bool checked)
{
  if (keysym == XK_Control_L) {
    menuCtrlKey = checked;
  } else if (keysym == XK_Alt_L) {
    menuAltKey = checked;
  }
}

bool Keyboard::getMenuCtrlKey() const
{
  return menuCtrlKey;
}

bool Keyboard::getMenuAltKey() const
{
  return menuAltKey;
}

void Keyboard::setContextMenuVisible(bool /*newContextMenuVisible*/)
{

}

bool Keyboard::isKeyboardGrabbed() const
{
  return keyboardGrabbed;
}
