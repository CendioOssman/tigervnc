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

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif

#include <X11/XKBlib.h>

#include <rfb/Exception.h>
#include <rfb/LogWriter.h>
#include <rfb/ledStates.h>

#define XK_MISCELLANY
#include <rfb/keysymdef.h>

#include "i18n.h"
#include "KeyboardX11.h"

extern const struct _code_map_xkb_to_qnum {
  const char * from;
  const unsigned short to;
} code_map_xkb_to_qnum[];
extern const unsigned int code_map_xkb_to_qnum_len;

static rfb::LogWriter vlog("KeyboardX11");

KeyboardX11::KeyboardX11(KeyboardHandler* handler_)
  : Keyboard(handler_)
{
  Display* dpy;
  XkbDescPtr xkb;
  Status status;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  XkbSetDetectableAutoRepeat(dpy, True, nullptr); // ported from vncviewer.cxx.

  xkb = XkbGetMap(dpy, 0, XkbUseCoreKbd);
  if (!xkb)
    throw rfb::Exception("XkbGetMap");

  status = XkbGetNames(dpy, XkbKeyNamesMask, xkb);
  if (status != Success)
    throw rfb::Exception("XkbGetNames");

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

bool KeyboardX11::handleEvent(const char* eventType, void* message)
{
  Display* dpy;
  const xcb_generic_event_t* xcbevent;

  assert(eventType);
  assert(message);

  if (strcmp(eventType, "xcb_generic_event_t") != 0)
    return false;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  xcbevent = (xcb_generic_event_t*)message;

  if (xcbevent->response_type == XCB_KEY_PRESS) {
    const xcb_key_press_event_t* xcbkey;

    int keycode;
    Bool ret;
    unsigned mods;
    KeySym keysym;

    xcbkey = (xcb_key_press_event_t*)xcbevent;

    keycode = code_map_keycode_to_qnum[xcbkey->detail];

    ret = XkbLookupKeySym(dpy, xcbkey->detail, xcbkey->state,
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

unsigned KeyboardX11::getLEDState()
{
  unsigned state;

  unsigned int mask;

  Display* dpy;
  Status status;
  XkbStateRec xkbState;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  status = XkbGetState(dpy, XkbUseCoreKbd, &xkbState);
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

  Display* dpy;
  Bool ret;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

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

  ret = XkbLockModifiers(dpy, XkbUseCoreKbd, affect, values);
  if (!ret)
    vlog.error(_("Failed to update keyboard LED state"));
}

unsigned KeyboardX11::getModifierMask(uint32_t keysym)
{
  Display* dpy;
  XkbDescPtr xkb;
  unsigned int mask, keycode;
  XkbAction *act;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  mask = 0;

  xkb = XkbGetMap(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
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
