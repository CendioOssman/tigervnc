#include "X11KeyboardHandler.h"

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

#include <QDebug>
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

static rfb::LogWriter vlog("X11KeyboardHandler");

Bool eventIsFocusWithSerial(Display* display, XEvent* event, XPointer arg)
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

X11KeyboardHandler::X11KeyboardHandler(QObject* parent)
  : BaseKeyboardHandler(parent)
{
  qDebug() << "X11KeyboardHandler";
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  display_ = QX11Info::display();
#else
  display_ = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  XkbSetDetectableAutoRepeat(display_, True, nullptr); // ported from vncviewer.cxx.

  XkbDescPtr xkb = XkbGetMap(display_, 0, XkbUseCoreKbd);
  if (!xkb) {
    throw rfb::Exception("XkbGetMap");
  }
  Status status = XkbGetNames(display_, XkbKeyNamesMask, xkb);
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
      // vlog.debug("No key mapping for key %.4s", keyname);
    }
  }

  XkbFreeKeyboard(xkb, 0, True);

  keyboardGrabberTimer.setInterval(500);
  keyboardGrabberTimer.setSingleShot(true);
  connect(&keyboardGrabberTimer, &QTimer::timeout, this, &X11KeyboardHandler::grabKeyboard);
}

X11KeyboardHandler::~X11KeyboardHandler()
{
  qDebug() << "~X11KeyboardHandler";
}

bool X11KeyboardHandler::nativeEventFilter(QByteArray const& eventType, void* message, long*)
{
  if (eventType == "xcb_generic_event_t") {
    xcb_generic_event_t* ev = static_cast<xcb_generic_event_t*>(message);
    uint16_t xcbEventType = ev->response_type;
    // qDebug() << "X11KeyboardHandler::nativeEvent: xcbEventType=" << xcbEventType << ",eventType=" << eventType;
    if (xcbEventType == XCB_KEY_PRESS) {
      xcb_key_press_event_t* xevent = reinterpret_cast<xcb_key_press_event_t*>(message);
      qDebug() << "X11KeyboardHandler::nativeEvent: XCB_KEY_PRESS: keycode=0x" << Qt::hex << xevent->detail
               << ", state=0x" << xevent->state << ", mapped_keycode=0x" << code_map_keycode_to_qnum[xevent->detail];

      int keycode = code_map_keycode_to_qnum[xevent->detail];

      if (keycode == 50) {
        keycode = 42;
      }

      // Generate a fake keycode just for tracking if we can't figure
      // out the proper one
      if (keycode == 0)
        keycode = 0x100 | xevent->detail;

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
      XLookupString(&kev, buffer, sizeof(buffer), &keysym, NULL);

      if (keysym == NoSymbol) {
        vlog.error(_("No symbol for key code %d (in the current state)"), (int)xevent->detail);
      }

      switch (keysym) {
      // For the first few years, there wasn't a good consensus on what the
      // Windows keys should be mapped to for X11. So we need to help out a
      // bit and map all variants to the same key...
      case XK_Hyper_L:
        keysym = XK_Super_L;
        break;
      case XK_Hyper_R:
        keysym = XK_Super_R;
        break;
        // There has been several variants for Shift-Tab over the years.
        // RFB states that we should always send a normal tab.
      case XK_ISO_Left_Tab:
        keysym = XK_Tab;
        break;
      }

      if (!handleKeyPress(keycode, keysym))
        return false;
      return true;
    } else if (xcbEventType == XCB_KEY_RELEASE) {
      xcb_key_release_event_t* xevent = reinterpret_cast<xcb_key_release_event_t*>(message);
      int keycode = code_map_keycode_to_qnum[xevent->detail]; // TODO: what's this table???
      // int keycode = xevent->detail;
      if (keycode == 0)
        keycode = 0x100 | xevent->detail;
      if (!handleKeyRelease(keycode))
        return false;
      return true;
    } else if (xcbEventType == XCB_EXPOSE) {
    } else {
      // qDebug() << "nativeEvent: eventtype=" << xcbEventType;
    }
  }
  return false;
}

bool X11KeyboardHandler::handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode)
{
  qDebug() << "X11KeyboardHandler::handleKeyPress: keyCode=" << keyCode << ", keySym=" << keySym;
  if (menuKeySym && keySym == menuKeySym) {
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

  // Because of the way keyboards work, we cannot expect to have the same
  // symbol on release as when pressed. This breaks the VNC protocol however,
  // so we need to keep track of what keysym a key _code_ generated on press
  // and send the same on release.
  downKeySym[keyCode] = keySym;

  vlog.debug("Key pressed: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(keySym), keySym);

  try {
    QVNCConnection* cc = AppManager::instance()->getConnection();
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

bool X11KeyboardHandler::handleKeyRelease(int keyCode)
{
  DownMap::iterator iter;

  if (ViewerConfig::config()->viewOnly())
    return false;

  iter = downKeySym.find(keyCode);
  if (iter == downKeySym.end()) {
    // These occur somewhat frequently so let's not spam them unless
    // logging is turned up.
    vlog.debug("Unexpected release of key code %d", keyCode);
    return false;
  }

  vlog.debug("Key released: 0x%04x => XK_%s (0x%04x)", keyCode, XKeysymToString(iter->second), iter->second);

  try {
    QVNCConnection* cc = AppManager::instance()->getConnection();
    if (keyCode > 0xff)
      emit cc->writeKeyEvent(iter->second, 0, false);
    else
      emit cc->writeKeyEvent(iter->second, keyCode, false);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    e.abort = true;
    throw;
  }

  downKeySym.erase(iter);

  return true;
}

void X11KeyboardHandler::setLEDState(unsigned int state)
{
  // qDebug() << "X11KeyboardHandler::setLEDState";
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

void X11KeyboardHandler::pushLEDState()
{
  // qDebug() << "X11KeyboardHandler::pushLEDState";
  QVNCConnection* cc = AppManager::instance()->getConnection();
  // Server support?
  rfb::ServerParams* server = AppManager::instance()->getConnection()->server();
  if (server->ledState() == rfb::ledUnknown) {
    return;
  }
  XkbStateRec xkbState;
  Status status = XkbGetState(display, XkbUseCoreKbd, &xkbState);
  if (status != Success) {
    vlog.error(_("Failed to get keyboard LED state: %d"), status);
    return;
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

void X11KeyboardHandler::grabKeyboard()
{
  qDebug() << "X11KeyboardHandler::grabKeyboard";
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
  BaseKeyboardHandler::grabKeyboard();
}

void X11KeyboardHandler::ungrabKeyboard()
{
  keyboardGrabberTimer.stop();
  XUngrabKeyboard(display, CurrentTime);
  BaseKeyboardHandler::ungrabKeyboard();
}

void X11KeyboardHandler::releaseKeyboard()
{
  // Intentionally do nothing, in order to prevent Qt (on X11) from releasing the keyboard on focus out.
}

unsigned int X11KeyboardHandler::getModifierMask(unsigned int keysym)
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
