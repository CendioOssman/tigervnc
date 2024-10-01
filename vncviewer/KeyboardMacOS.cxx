#include "KeyboardMacOS.h"

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
#include "DesktopWindow.h"

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

static rfb::LogWriter vlog("KeyboardMacOS");

KeyboardMacOS::KeyboardMacOS(KeyboardHandler* handler_, QObject* parent)
  : Keyboard(handler_, parent)
{
}

bool KeyboardMacOS::handleEvent(const char* eventType, void* message)
{
  if (strcmp(eventType, "mac_generic_NSEvent") == 0) {
    if (cocoa_is_keyboard_event(message)) {
      int systemKeyCode = cocoa_event_keycode(message);
      uint32_t keyCode;
      if ((unsigned)systemKeyCode >= code_map_osx_to_qnum_len) {
        keyCode = 0;
      } else {
        keyCode = code_map_osx_to_qnum[systemKeyCode];
      }
      if (cocoa_is_key_press(message)) {
        uint32_t keySym = cocoa_event_keysym(message);
        if (keySym == NoSymbol) {
          vlog.error(_("No symbol for key code 0x%02x (in the current state)"), (int)keyCode);
        }

        handler->handleKeyPress(systemKeyCode, keyCode, keySym);

        // We don't get any release events for CapsLock, so we have to
        // send the release right away.
        if (keySym == XK_Caps_Lock) {
          handler->handleKeyRelease(systemKeyCode);
        }
      } else {
        handler->handleKeyRelease(systemKeyCode);
      }
      return true;
    }
  }
  return false;
}

bool KeyboardMacOS::isKeyboardSync(QByteArray const& eventType, void* message)
{
  if (strcmp(eventType, "mac_generic_NSEvent") == 0) {
    if (cocoa_is_keyboard_sync(message)) {
      return true;
    }
  }
  return false;
}

unsigned KeyboardMacOS::getLEDState()
{
  bool on;
  int ret = cocoa_get_caps_lock_state(&on);
  if (ret != 0) {
    vlog.error(_("Failed to get keyboard LED state: %d"), ret);
    return rfb::ledUnknown;
  }
  unsigned int state = 0;
  if (on) {
    state |= rfb::ledCapsLock;
  }
  ret = cocoa_get_num_lock_state(&on);
  if (ret != 0) {
    vlog.error(_("Failed to get keyboard LED state: %d"), ret);
    return rfb::ledUnknown;
  }
  if (on) {
    state |= rfb::ledNumLock;
  }
  return state;
}

void KeyboardMacOS::setLEDState(unsigned state)
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

void KeyboardMacOS::grabKeyboard()
{
  int ret = cocoa_capture_displays(AppManager::instance()->getWindow()->fullscreenScreens());
  if (ret == 1) {
      vlog.error(_("Failure grabbing keyboard"));
      return;
  }
  Keyboard::grabKeyboard();
}

void KeyboardMacOS::ungrabKeyboard()
{
  cocoa_release_displays();
  Keyboard::ungrabKeyboard();
}
