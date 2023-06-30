/* Copyright 2019 Aaron Sowry for Cendio AB
 * Copyright 2019-2020 Pierre Ossman for Cendio AB
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
#include <string.h>

#include <QDebug>
#include <QDataStream>
#include <QCursor>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif

#include <X11/XKBlib.h>

#ifndef XK_MISCELLANY
#define XK_MISCELLANY
#include <rfb/keysymdef.h>
#endif
#include <rfb/LogWriter.h>

#include "i18n.h"
#include "appmanager.h"
#include "abstractvncview.h"
#include "GestureHandler.h"
#include "X11TouchHandler.h"

static rfb::LogWriter vlog("XInputTouchHandler");

static Display *xdisplay()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  Display *display = QX11Info::display();
#else
  Display *display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  return display;
}

X11TouchHandler::X11TouchHandler(GestureHandler *gh)
  : fakeStateMask(0)
  , gestureHandler(gh) 
{
}

void X11TouchHandler::preparePointerEvent(XEvent* dst, const GestureEvent src)
{
  Window root, child;
  int rootX, rootY;
  XkbStateRec state;

  // We don't have a real event to steal things from, so we'll have
  // to fake these events based on the current state of things

  Window wnd = AppManager::instance()->view()->nativeWindowHandle();
  root = XDefaultRootWindow(xdisplay());
  XTranslateCoordinates(xdisplay(), wnd, root,
                        src.eventX,
                        src.eventY,
                        &rootX, &rootY, &child);
  XkbGetState(xdisplay(), XkbUseCoreKbd, &state);

  // XButtonEvent and XMotionEvent are almost identical, so we
  // don't have to care which it is for these fields
  dst->xbutton.serial = XLastKnownRequestProcessed(xdisplay());
  dst->xbutton.display = xdisplay();
  dst->xbutton.window = wnd;
  dst->xbutton.root = root;
  dst->xbutton.subwindow = None;
  dst->xbutton.time = CurrentTime;
  dst->xbutton.x = src.eventX;
  dst->xbutton.y = src.eventY;
  dst->xbutton.x_root = rootX;
  dst->xbutton.y_root = rootY;
  dst->xbutton.state = state.mods;
  dst->xbutton.state |= ((state.ptr_buttons >> 1) & 0x1f) << 8;
  dst->xbutton.same_screen = True;
}

void X11TouchHandler::fakeMotionEvent(const GestureEvent origEvent)
{
  XEvent fakeEvent;

  memset(&fakeEvent, 0, sizeof(XEvent));

  fakeEvent.type = MotionNotify;
  fakeEvent.xmotion.is_hint = False;
  preparePointerEvent(&fakeEvent, origEvent);

  fakeEvent.xbutton.state |= fakeStateMask;

  pushFakeEvent(&fakeEvent);
}

void X11TouchHandler::fakeButtonEvent(bool press, int button,
                                         const GestureEvent origEvent)
{
  XEvent fakeEvent;

  memset(&fakeEvent, 0, sizeof(XEvent));

  fakeEvent.type = press ? ButtonPress : ButtonRelease;
  fakeEvent.xbutton.button = button;
  preparePointerEvent(&fakeEvent, origEvent);

  fakeEvent.xbutton.state |= fakeStateMask;

  // The button mask should indicate the button state just prior to
  // the event, we update the button mask after pushing
  pushFakeEvent(&fakeEvent);

  // Set/unset the bit for the correct button
  if (press) {
    fakeStateMask |= (1<<(7+button));
  } else {
    fakeStateMask &= ~(1<<(7+button));
  }
}

void X11TouchHandler::fakeKeyEvent(bool press, int keysym,
                                      const GestureEvent origEvent)
{
  XEvent fakeEvent;

  Window root, child;
  int rootX, rootY;
  XkbStateRec state;

  int modmask;

  Window wnd = AppManager::instance()->view()->nativeWindowHandle();
  root = XDefaultRootWindow(xdisplay());
  XTranslateCoordinates(xdisplay(), wnd, root,
                        origEvent.eventX,
                        origEvent.eventY,
                        &rootX, &rootY, &child);
  XkbGetState(xdisplay(), XkbUseCoreKbd, &state);

  KeyCode kc = XKeysymToKeycode(xdisplay(), keysym);

  memset(&fakeEvent, 0, sizeof(XEvent));

  fakeEvent.type = press ? KeyPress : KeyRelease;
  fakeEvent.xkey.type = press ? KeyPress : KeyRelease;
  fakeEvent.xkey.keycode = kc;
  fakeEvent.xkey.serial = XLastKnownRequestProcessed(xdisplay());
  fakeEvent.xkey.display = xdisplay();
  fakeEvent.xkey.window = wnd;
  fakeEvent.xkey.root = root;
  fakeEvent.xkey.subwindow = None;
  fakeEvent.xkey.time = CurrentTime;
  fakeEvent.xkey.x = origEvent.eventX;
  fakeEvent.xkey.y = origEvent.eventY;
  fakeEvent.xkey.x_root = rootX;
  fakeEvent.xkey.y_root = rootY;
  fakeEvent.xkey.state = state.mods;
  fakeEvent.xkey.state |= ((state.ptr_buttons >> 1) & 0x1f) << 8;
  fakeEvent.xkey.same_screen = True;

  // Apply our fake mask
  fakeEvent.xkey.state |= fakeStateMask;

  pushFakeEvent(&fakeEvent);

  switch(keysym) {
    case XK_Shift_L:
    case XK_Shift_R:
      modmask = ShiftMask;
      break;
    case XK_Caps_Lock:
      modmask = LockMask;
      break;
    case XK_Control_L:
    case XK_Control_R:
      modmask = ControlMask;
      break;
    default:
      modmask = 0;
  }

  if (press)
    fakeStateMask |= modmask;
  else
    fakeStateMask &= ~modmask;
}

void X11TouchHandler::pushFakeEvent(XEvent* event)
{
  // Perhaps use XPutBackEvent() to avoid round trip latency?
  XSendEvent(xdisplay(), event->xany.window, true, 0, event);
}
