#include "MacKeyboardHandler.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "PlatformPixelBuffer.h"
#include "appmanager.h"
#include "cocoa.h"
#include "i18n.h"
#include "parameters.h"
#include "rdr/Exception.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/ledStates.h"
#include "vncconnection.h"

#include <QDebug>
extern const unsigned short code_map_osx_to_qnum[];
extern const unsigned int code_map_osx_to_qnum_len;

#ifndef XK_VoidSymbol
#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include <rfb/keysymdef.h>
#endif

#ifndef NoSymbol
#define NoSymbol 0
#endif

static rfb::LogWriter vlog("MacKeyboardHandler");

MacKeyboardHandler::MacKeyboardHandler(QObject* parent)
  : BaseKeyboardHandler(parent)
{
}

bool MacKeyboardHandler::nativeEventFilter(const QByteArray& eventType, void* message, long* result)
{
  Q_UNUSED(result)
  if (eventType == "mac_generic_NSEvent") {
    if (cocoa_is_keyboard_sync(message)) {
      while (!downKeySym_.empty()) {
        handleKeyRelease(downKeySym_.begin()->first);
      }
      return true;
    }
    if (cocoa_is_keyboard_event(message)) {
      int keyCode = cocoa_event_keycode(message);
      // qDebug() << "nativeEvent: keyEvent: keyCode=" << keyCode << ", hexKeyCode=" << Qt::hex << keyCode;
      if ((unsigned)keyCode >= code_map_osx_to_qnum_len) {
        keyCode = 0;
      } else {
        keyCode = code_map_osx_to_qnum[keyCode];
      }
      if (cocoa_is_key_press(message)) {
        uint32_t keySym = cocoa_event_keysym(message);
        if (keySym == NoSymbol) {
          vlog.error(_("No symbol for key code 0x%02x (in the current state)"), (int)keyCode);
        }

        if (!handleKeyPress(keyCode, keySym))
          return false;

        // We don't get any release events for CapsLock, so we have to
        // send the release right away.
        if (keySym == XK_Caps_Lock) {
          handleKeyRelease(keyCode);
        }
      } else {
        if (!handleKeyRelease(keyCode))
          return false;
      }
      return true;
    }
  }
  return false;
}

bool MacKeyboardHandler::handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode)
{
  if (menuKeySym_ && keySym == menuKeySym_) {
    if (!menuShortCutMode) {
      emit contextMenuKeyPressed(menuShortCutMode);
      return true;
    }
  }

  if (ViewerConfig::config()->viewOnly())
    return false;

  if (keyCode == 0) {
    vlog.error(_("No key code specified on key press"));
    return false;
  }

  // Alt on OS X behaves more like AltGr on other systems, and to get
  // sane behaviour we should translate things in that manner for the
  // remote VNC server. However that means we lose the ability to use
  // Alt as a shortcut modifier. Do what RealVNC does and hijack the
  // left command key as an Alt replacement.
  switch (keySym) {
  case XK_Super_L:
    keySym = XK_Alt_L;
    break;
  case XK_Super_R:
    keySym = XK_Super_L;
    break;
  case XK_Alt_L:
    keySym = XK_Mode_switch;
    break;
  case XK_Alt_R:
    keySym = XK_ISO_Level3_Shift;
    break;
  }

  // Because of the way keyboards work, we cannot expect to have the same
  // symbol on release as when pressed. This breaks the VNC protocol however,
  // so we need to keep track of what keysym a key _code_ generated on press
  // and send the same on release.
  downKeySym_[keyCode] = keySym;

  //  vlog.debug("Key pressed: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(keySym), keySym);

  try {
    QVNCConnection* cc = AppManager::instance()->connection();
    // Fake keycode?
    if (keyCode > 0xff)
      emit cc->writeKeyEvent(keySym, 0, true);
    else
      emit cc->writeKeyEvent(keySym, keyCode, true);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }

  return true;
}

bool MacKeyboardHandler::handleKeyRelease(int keyCode)
{
  DownMap::iterator iter;

  if (ViewerConfig::config()->viewOnly())
    return false;

  iter = downKeySym_.find(keyCode);
  if (iter == downKeySym_.end()) {
    // These occur somewhat frequently so let's not spam them unless
    // logging is turned up.
    vlog.debug("Unexpected release of key code %d", keyCode);
    return false;
  }

  //  vlog.debug("Key released: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(iter->second), iter->second);

  try {
    QVNCConnection* cc = AppManager::instance()->connection();
    if (keyCode > 0xff)
      emit cc->writeKeyEvent(iter->second, 0, false);
    else
      emit cc->writeKeyEvent(iter->second, keyCode, false);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }

  downKeySym_.erase(iter);

  return true;
}

void MacKeyboardHandler::setLEDState(unsigned int state)
{
  // qDebug() << "MacKeyboardHandler::setLEDState";
  vlog.debug("Got server LED state: 0x%08x", state);

  // The first message is just considered to be the server announcing
  // support for this extension. We will push our state to sync up the
  // server when we get focus. If we already have focus we need to push
  // it here though.
  // if (firstLEDState_) {
  //     firstLEDState_ = false;
  //     if (hasFocus()) {
  //         pushLEDState();
  //     }
  //     return;
  // }

  // if (!hasFocus()) {
  //     return;
  // }

  int ret = cocoa_set_caps_lock_state(state & rfb::ledCapsLock);
  if (ret == 0) {
    ret = cocoa_set_num_lock_state(state & rfb::ledNumLock);
  }

  if (ret != 0) {
    vlog.error(_("Failed to update keyboard LED state: %d"), ret);
  }
}

void MacKeyboardHandler::pushLEDState()
{
  // qDebug() << "MacKeyboardHandler::pushLEDState";
  QVNCConnection* cc = AppManager::instance()->connection();
  // Server support?
  rfb::ServerParams* server = AppManager::instance()->connection()->server();
  if (server->ledState() == rfb::ledUnknown) {
    return;
  }

  bool on;
  int ret = cocoa_get_caps_lock_state(&on);
  if (ret != 0) {
    vlog.error(_("Failed to get keyboard LED state: %d"), ret);
    return;
  }
  unsigned int state = 0;
  if (on) {
    state |= rfb::ledCapsLock;
  }
  ret = cocoa_get_num_lock_state(&on);
  if (ret != 0) {
    vlog.error(_("Failed to get keyboard LED state: %d"), ret);
    return;
  }
  if (on) {
    state |= rfb::ledNumLock;
  }
  // No support for Scroll Lock //
  state |= (cc->server()->ledState() & rfb::ledScrollLock);
  if ((state & rfb::ledCapsLock) != (cc->server()->ledState() & rfb::ledCapsLock)) {
    vlog.debug("Inserting fake CapsLock to get in sync with server");
    handleKeyPress(0x3a, XK_Caps_Lock);
    handleKeyRelease(0x3a);
  }
  if ((state & rfb::ledNumLock) != (cc->server()->ledState() & rfb::ledNumLock)) {
    vlog.debug("Inserting fake NumLock to get in sync with server");
    handleKeyPress(0x45, XK_Num_Lock);
    handleKeyRelease(0x45);
  }
  if ((state & rfb::ledScrollLock) != (cc->server()->ledState() & rfb::ledScrollLock)) {
    vlog.debug("Inserting fake ScrollLock to get in sync with server");
    handleKeyPress(0x46, XK_Scroll_Lock);
    handleKeyRelease(0x46);
  }
}

void MacKeyboardHandler::grabKeyboard()
{
  // int ret = cocoa_capture_displays(view_, fullscreenScreens());
  // if (ret != 0) {
  //     vlog.error(_("Failure grabbing keyboard"));
  //     return;
  // }
  BaseKeyboardHandler::grabKeyboard();
}

void MacKeyboardHandler::ungrabKeyboard()
{
  // cocoa_release_displays(view_, fullscreenEnabled_);
  BaseKeyboardHandler::ungrabKeyboard();
}
