/* Copyright 2011-2021 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include <algorithm>
#include <stdexcept>

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif

#include <X11/XKBlib.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include <core/LogWriter.h>
#include <core/i18n.h>

#include <rfb/ledStates.h>

#include "KeyboardX11.h"

extern const struct _code_map_xkb_to_qnum {
  const char * from;
  const unsigned short to;
} code_map_xkb_to_qnum[];
extern const unsigned int code_map_xkb_to_qnum_len;

static core::LogWriter vlog("KeyboardX11");

static Display* qt_display()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  return QX11Info::display();
#else
  return qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
}

KeyboardX11::KeyboardX11(KeyboardHandler* handler_)
  : Keyboard(handler_)
{
  Display* display;
  XkbDescPtr xkb;
  Status status;

  display = qt_display();

  XkbSetDetectableAutoRepeat(display, True, nullptr);

  xkb = XkbGetMap(display, 0, XkbUseCoreKbd);
  if (!xkb)
    throw std::runtime_error("XkbGetMap");

  status = XkbGetNames(display, XkbKeyNamesMask, xkb);
  if (status != Success)
    throw std::runtime_error("XkbGetNames");

  memset(code_map_keycode_to_qnum, 0, sizeof(code_map_keycode_to_qnum));
  for (KeyCode keycode = xkb->min_key_code;
       keycode < xkb->max_key_code;
       keycode++) {
    const char *keyname = xkb->names->keys[keycode].name;
    unsigned short rfbcode;

    if (keyname[0] == '\0')
      continue;

    rfbcode = 0;
    for (unsigned i = 0;i < code_map_xkb_to_qnum_len;i++) {
        if (strncmp(code_map_xkb_to_qnum[i].from,
                    keyname, XkbKeyNameLength) == 0) {
            rfbcode = code_map_xkb_to_qnum[i].to;
            break;
        }
    }
    if (rfbcode != 0)
        code_map_keycode_to_qnum[keycode] = rfbcode;
    else
        vlog.debug("No key mapping for key %.4s", keyname);
  }

  XkbFreeKeyboard(xkb, 0, True);
}

KeyboardX11::~KeyboardX11()
{
}

struct GrabInfo {
  Window window;
  bool found;
};

bool KeyboardX11::handleEvent(const char* eventType, void* message)
{
  Display* display;
  const xcb_generic_event_t* xcbevent;

  assert(eventType);
  assert(message);

  if (strcmp(eventType, "xcb_generic_event_t") != 0)
    return false;

  display = qt_display();

  xcbevent = (xcb_generic_event_t*)message;

  if (xcbevent->response_type == XCB_KEY_PRESS) {
    const xcb_key_press_event_t* xcbkey;

    int keycode;
    Bool ret;
    unsigned mods;
    KeySym keysym;

    xcbkey = (xcb_key_press_event_t*)xcbevent;

    keycode = code_map_keycode_to_qnum[xcbkey->detail];

    ret = XkbLookupKeySym(display, xcbkey->detail, xcbkey->state,
                          &mods, &keysym);
    if (!ret || (keysym == NoSymbol)) {
      vlog.error(_("No symbol for key code %d (in the current state)"),
                 (int)xcbkey->detail);
    }

    handler->handleKeyPress(xcbkey->detail, keycode, keysym);
    return true;
  } else if (xcbevent->response_type == XCB_KEY_RELEASE) {
    const xcb_key_release_event_t* xcbkey;
    xcbkey = (xcb_key_release_event_t*)xcbevent;
    handler->handleKeyRelease(xcbkey->detail);
    return true;
  }

  return false;
}

std::list<uint32_t> KeyboardX11::translateToKeySyms(int systemKeyCode)
{
  Status status;
  XkbStateRec state;
  std::list<uint32_t> keySyms;
  unsigned char group;

  status = XkbGetState(qt_display(), XkbUseCoreKbd, &state);
  if (status != Success)
    return keySyms;

  // Start with the currently used group
  translateToKeySyms(systemKeyCode, state.group, &keySyms);

  // Then all other groups
  for (group = 0; group < XkbNumKbdGroups; group++) {
    if (group == state.group)
      continue;

    translateToKeySyms(systemKeyCode, group, &keySyms);
  }

  return keySyms;
}

unsigned KeyboardX11::getLEDState()
{
  unsigned state;

  unsigned int mask;

  Display* display;
  Status status;
  XkbStateRec xkbState;

  display = qt_display();

  status = XkbGetState(display, XkbUseCoreKbd, &xkbState);
  if (status != Success) {
    vlog.error(_("Failed to get keyboard LED state: %d"), status);
    return rfb::ledUnknown;
  }

  state = 0;

  if (xkbState.locked_mods & LockMask)
    state |= rfb::ledCapsLock;

  mask = getModifierMask(XK_Num_Lock);
  if (xkbState.locked_mods & mask)
    state |= rfb::ledNumLock;

  mask = getModifierMask(XK_Scroll_Lock);
  if (xkbState.locked_mods & mask)
    state |= rfb::ledScrollLock;

  return state;
}

void KeyboardX11::setLEDState(unsigned state)
{
  unsigned int affect, values;
  unsigned int mask;

  Display* display;
  Bool ret;

  display = qt_display();

  affect = values = 0;

  affect |= LockMask;
  if (state & rfb::ledCapsLock)
    values |= LockMask;

  mask = getModifierMask(XK_Num_Lock);
  affect |= mask;
  if (state & rfb::ledNumLock)
    values |= mask;

  mask = getModifierMask(XK_Scroll_Lock);
  affect |= mask;
  if (state & rfb::ledScrollLock)
    values |= mask;

  ret = XkbLockModifiers(display, XkbUseCoreKbd, affect, values);
  if (!ret)
    vlog.error(_("Failed to update keyboard LED state: %d"), ret);
}

unsigned KeyboardX11::getModifierMask(uint32_t keysym)
{
  Display* display;
  XkbDescPtr xkb;
  unsigned int mask, keycode;
  XkbAction *act;

  display = qt_display();

  mask = 0;

  xkb = XkbGetMap(display, XkbAllComponentsMask, XkbUseCoreKbd);
  if (xkb == nullptr)
    return 0;

  for (keycode = xkb->min_key_code; keycode <= xkb->max_key_code; keycode++) {
    unsigned int state_out;
    KeySym ks;

    XkbTranslateKeyCode(xkb, keycode, 0, &state_out, &ks);
    if (ks == NoSymbol)
      continue;

    if (ks == keysym)
      break;
  }

  // KeySym not mapped?
  if (keycode > xkb->max_key_code)
    goto out;

  act = XkbKeyAction(xkb, keycode, 0);
  if (act == nullptr)
    goto out;
  if (act->type != XkbSA_LockMods)
    goto out;

  if (act->mods.flags & XkbSA_UseModMapMods)
    mask = xkb->map->modmap[keycode];
  else
    mask = act->mods.mask;

out:
  XkbFreeKeyboard(xkb, XkbAllComponentsMask, True);

  return mask;
}

void KeyboardX11::translateToKeySyms(int systemKeyCode,
                                     unsigned char group,
                                     std::list<uint32_t>* keySyms)
{
  unsigned int mods;

  // Start with no modifiers
  translateToKeySyms(systemKeyCode, group, 0, keySyms);

  // Next just a single modifier at a time
  for (mods = 1; mods < (Mod5Mask+1); mods <<= 1)
    translateToKeySyms(systemKeyCode, group, mods, keySyms);

  // Finally everything
  for (mods = 0; mods < (Mod5Mask<<1); mods++)
    translateToKeySyms(systemKeyCode, group, mods, keySyms);
}

void KeyboardX11::translateToKeySyms(int systemKeyCode,
                                     unsigned char group,
                                     unsigned char mods,
                                     std::list<uint32_t>* keySyms)
{
  KeySym ks;
  std::list<uint32_t>::const_iterator iter;

  ks = XkbKeycodeToKeysym(qt_display(), systemKeyCode, group, mods);
  if (ks == NoSymbol)
    return;

  iter = std::find(keySyms->begin(), keySyms->end(), ks);
  if (iter != keySyms->end())
    return;

  keySyms->push_back(ks);
}
