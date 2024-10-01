#include "KeyboardX11.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif

#include "PlatformPixelBuffer.h"
#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "rfb/CMsgWriter.h"
#include "rfb/Exception.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/ledStates.h"
#include "vncconnection.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xrender.h>

extern const struct _code_map_xkb_to_qnum {
  char const* from;
  unsigned short const to;
} code_map_xkb_to_qnum[];

extern unsigned int const code_map_xkb_to_qnum_len;

static int code_map_keycode_to_qnum[256];

static rfb::LogWriter vlog("KeyboardX11");

Bool eventIsFocusWithSerial(Display* /*dpy*/, XEvent* event, XPointer arg)
{
  unsigned long serial = *(unsigned long*)arg;
  if (event->xany.serial != serial) {
    return False;
  }
  if ((event->type != FocusIn) && (event->type != FocusOut)) {
    return False;
  }
  return True;
}

KeyboardX11::KeyboardX11(KeyboardHandler* handler_, QObject* parent)
  : Keyboard(handler_, parent)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  display = QX11Info::display();
#else
  display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  XkbSetDetectableAutoRepeat(display, True, nullptr); // ported from vncviewer.cxx.

  XkbDescPtr xkb = XkbGetMap(display, 0, XkbUseCoreKbd);
  if (!xkb) {
    throw rfb::Exception("XkbGetMap");
  }
  Status status = XkbGetNames(display, XkbKeyNamesMask, xkb);
  if (status != Success) {
    throw rfb::Exception("XkbGetNames");
  }
  memset(code_map_keycode_to_qnum, 0, sizeof(code_map_keycode_to_qnum));
  for (KeyCode keycode = xkb->min_key_code; keycode < xkb->max_key_code; keycode++) {
    char const* keyname = xkb->names->keys[keycode].name;
    if (keyname[0] == '\0') {
      continue;
    }
    unsigned short rfbcode = 0;
    for (unsigned i = 0; i < code_map_xkb_to_qnum_len; i++) {
      if (strncmp(code_map_xkb_to_qnum[i].from, keyname, XkbKeyNameLength) == 0) {
        rfbcode = code_map_xkb_to_qnum[i].to;
        break;
      }
    }
    if (rfbcode != 0) {
      code_map_keycode_to_qnum[keycode] = rfbcode;
    } else {
      code_map_keycode_to_qnum[keycode] = keycode;
      vlog.debug("No key mapping for key %.4s", keyname);
    }
  }

  XkbFreeKeyboard(xkb, 0, True);

  keyboardGrabberTimer.setInterval(500);
  keyboardGrabberTimer.setSingleShot(true);
  connect(&keyboardGrabberTimer, &QTimer::timeout, this, &KeyboardX11::grabKeyboard);
}

KeyboardX11::~KeyboardX11()
{

}

bool KeyboardX11::handleEvent(const char* eventType, void* message)
{
  if (strcmp(eventType, "xcb_generic_event_t") == 0) {
    xcb_generic_event_t* ev = static_cast<xcb_generic_event_t*>(message);
    uint16_t xcbEventType = ev->response_type;
    if (xcbEventType == XCB_KEY_PRESS) {
      xcb_key_press_event_t* xevent = reinterpret_cast<xcb_key_press_event_t*>(message);

      int keycode = code_map_keycode_to_qnum[xevent->detail];

      if (keycode == 50) {
        keycode = 42;
      }

      XKeyEvent kev;
      kev.type = xevent->response_type;
      kev.serial = xevent->sequence;
      kev.send_event = false;
      kev.display = display;
      kev.window = xevent->event;
      kev.root = xevent->root;
      kev.subwindow = xevent->child;
      kev.time = xevent->time;
      kev.x = xevent->event_x;
      kev.y = xevent->event_y;
      kev.x_root = xevent->root_x;
      kev.y_root = xevent->root_y;
      kev.state = xevent->state;
      kev.keycode = xevent->detail;
      kev.same_screen = xevent->same_screen;
      char buffer[10];
      KeySym keysym;
      XLookupString(&kev, buffer, sizeof(buffer), &keysym, nullptr);

      if (keysym == NoSymbol) {
        vlog.error(_("No symbol for key code %d (in the current state)"), (int)xevent->detail);
      }

      handler->handleKeyPress(xevent->detail, keycode, keysym);
      return true;
    } else if (xcbEventType == XCB_KEY_RELEASE) {
      xcb_key_release_event_t* xevent = reinterpret_cast<xcb_key_release_event_t*>(message);
      handler->handleKeyRelease(xevent->detail);
      return true;
    }
  }
  return false;
}

unsigned KeyboardX11::getLEDState()
{
  XkbStateRec xkbState;
  Status status = XkbGetState(display, XkbUseCoreKbd, &xkbState);
  if (status != Success) {
    vlog.error(_("Failed to get keyboard LED state: %d"), status);
    return rfb::ledUnknown;
  }
  unsigned int state = 0;
  if (xkbState.locked_mods & LockMask) {
    state |= rfb::ledCapsLock;
  }
  unsigned int mask = getModifierMask(XK_Num_Lock);
  if (xkbState.locked_mods & mask) {
    state |= rfb::ledNumLock;
  }
  mask = getModifierMask(XK_Scroll_Lock);
  if (xkbState.locked_mods & mask) {
    state |= rfb::ledScrollLock;
  }

  return state;
}

void KeyboardX11::setLEDState(unsigned state)
{
  vlog.debug("Got server LED state: 0x%08x", state);

  unsigned int affect = 0;
  unsigned int values = 0;

  affect |= LockMask;
  if (state & rfb::ledCapsLock) {
    values |= LockMask;
  }
  unsigned int mask = getModifierMask(XK_Num_Lock);
  affect |= mask;
  if (state & rfb::ledNumLock) {
    values |= mask;
  }
  mask = getModifierMask(XK_Scroll_Lock);
  affect |= mask;
  if (state & rfb::ledScrollLock) {
    values |= mask;
  }
  Bool ret = XkbLockModifiers(display, XkbUseCoreKbd, affect, values);
  if (!ret) {
    vlog.error(_("Failed to update keyboard LED state"));
  }
}

void KeyboardX11::grabKeyboard()
{
  keyboardGrabberTimer.stop();
  Window w;
  int revert_to;
  XGetInputFocus(display, &w, &revert_to);
  int ret = XGrabKeyboard(display, w, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  if (ret) {
    if (ret == AlreadyGrabbed) {
      // It seems like we can race with the WM in some cases.
      // Try again in a bit.
      keyboardGrabberTimer.start();
    } else {
      vlog.error(_("Failure grabbing keyboard"));
    }
    return;
  }

  // Xorg 1.20+ generates FocusIn/FocusOut even when there is no actual
  // change of focus. This causes us to get stuck in an endless loop
  // grabbing and ungrabbing the keyboard. Avoid this by filtering out
  // any focus events generated by XGrabKeyboard().
  XSync(display, False);
  XEvent xev;
  unsigned long serial;
  while (XCheckIfEvent(display, &xev, &eventIsFocusWithSerial, (XPointer)&serial) == True) {
    vlog.debug("Ignored synthetic focus event cause by grab change");
  }
  Keyboard::grabKeyboard();
}

void KeyboardX11::ungrabKeyboard()
{
  keyboardGrabberTimer.stop();
  XUngrabKeyboard(display, CurrentTime);
  Keyboard::ungrabKeyboard();
}

unsigned int KeyboardX11::getModifierMask(unsigned int keysym)
{
  XkbDescPtr xkb = XkbGetMap(display, XkbAllComponentsMask, XkbUseCoreKbd);
  if (xkb == nullptr) {
    return 0;
  }
  unsigned int keycode;
  for (keycode = xkb->min_key_code; keycode <= xkb->max_key_code; keycode++) {
    unsigned int state_out;
    KeySym ks;
    XkbTranslateKeyCode(xkb, keycode, 0, &state_out, &ks);
    if (ks == NoSymbol) {
      continue;
    }
    if (ks == keysym) {
      break;
    }
  }

  // KeySym not mapped?
  if (keycode > xkb->max_key_code) {
    XkbFreeKeyboard(xkb, XkbAllComponentsMask, True);
    return 0;
  }
  XkbAction* act = XkbKeyAction(xkb, keycode, 0);
  if (act == nullptr) {
    XkbFreeKeyboard(xkb, XkbAllComponentsMask, True);
    return 0;
  }
  if (act->type != XkbSA_LockMods) {
    XkbFreeKeyboard(xkb, XkbAllComponentsMask, True);
    return 0;
  }

  unsigned int mask = 0;
  if (act->mods.flags & XkbSA_UseModMapMods) {
    mask = xkb->map->modmap[keycode];
  } else {
    mask = act->mods.mask;
  }
  XkbFreeKeyboard(xkb, XkbAllComponentsMask, True);
  return mask;
}
