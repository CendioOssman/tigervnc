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
#include "vncwindow.h"

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
      while (!downKeySym.empty()) {
        handleKeyRelease(downKeySym.begin()->first);
      }
      return true;
    }
    if (cocoa_is_keyboard_event(message)) {
      int keyCode = cocoa_event_keycode(message);
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

  return BaseKeyboardHandler::handleKeyPress(keyCode, keySym, menuShortCutMode);
}

void MacKeyboardHandler::setLEDState(unsigned int state)
{
  vlog.debug("Got server LED state: 0x%08x", state);

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
  QVNCConnection* cc = AppManager::instance()->getConnection();
  // Server support?
  rfb::ServerParams* server = AppManager::instance()->getConnection()->server();
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
  int ret = cocoa_capture_displays(AppManager::instance()->getWindow()->fullscreenScreens());
  if (ret == 1) {
      vlog.error(_("Failure grabbing keyboard"));
      return;
  }
  BaseKeyboardHandler::grabKeyboard();
}

void MacKeyboardHandler::ungrabKeyboard()
{
  cocoa_release_displays();
  BaseKeyboardHandler::ungrabKeyboard();
}
