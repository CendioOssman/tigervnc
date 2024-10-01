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

Keyboard::Keyboard(QObject* parent)
  : QObject(parent)
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

bool Keyboard::handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode)
{
  vlog.debug("Keyboard::handleKeyPress");

  int menuKeyCode;
  quint32 menuKeySym;
  ::getMenuKey(&menuKeyCode, &menuKeySym);

  if (menuKeySym && keySym == menuKeySym) {
    if (!menuShortCutMode) {
      emit contextMenuKeyPressed(menuShortCutMode);
      return true;
    }
  }

  if (::viewOnly)
    return false;

  if (keyCode == 0) {
    vlog.error(_("No key code specified on key press"));
    return false;
  }

  // Because of the way keyboards work, we cannot expect to have the same
  // symbol on release as when pressed. This breaks the VNC protocol however,
  // so we need to keep track of what keysym a key _code_ generated on press
  // and send the same on release.
  downKeySym[keyCode] = keySym;

#if defined(WIN32) || defined(__APPLE__)
  vlog.debug("Key pressed: 0x%04x => 0x%04x", keyCode, keySym);
#else
  vlog.debug("Key pressed: 0x%04x => XK_%s (0x%04x)",
             keyCode, XKeysymToString(keySym), keySym);
#endif


  try {
    QVNCConnection* cc = AppManager::instance()->getConnection();
    // Fake keycode?
    if (keyCode > 0xff)
      emit cc->writeKeyEvent(keySym, 0, true);
    else
      emit cc->writeKeyEvent(keySym, keyCode, true);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    throw;
  }

  return true;
}

bool Keyboard::handleKeyRelease(int keyCode)
{
  vlog.debug("Keyboard::handleKeyRelease");
  DownMap::iterator iter;

  if (::viewOnly)
    return false;

  iter = downKeySym.find(keyCode);
  if (iter == downKeySym.end()) {
    // These occur somewhat frequently so let's not spam them unless
    // logging is turned up.
    vlog.debug("Unexpected release of key code %d", keyCode);
    return false;
  }

#if defined(WIN32) || defined(__APPLE__)
  vlog.debug("Key released: 0x%04x => 0x%04x", keyCode, iter->second);
#else
  vlog.debug("Key released: 0x%04x => XK_%s (0x%04x)",
             keyCode, XKeysymToString(iter->second), iter->second);
#endif

  try {
    QVNCConnection* cc = AppManager::instance()->getConnection();
    if (keyCode > 0xff)
      emit cc->writeKeyEvent(iter->second, 0, false);
    else
      emit cc->writeKeyEvent(iter->second, keyCode, false);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    throw;
  }

  downKeySym.erase(iter);

  return true;
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

void Keyboard::resetKeyboard()
{
  while (!downKeySym.empty()) {
    handleKeyRelease(downKeySym.begin()->first);
  }
}
