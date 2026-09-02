/* Copyright 2019 Aaron Sowry for Cendio AB
 * Copyright 2019-2026 Pierre Ossman for Cendio AB
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

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif

#include <xcb/xcb.h>
#include <xcb/xinput.h>

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2.h>
#include <X11/XKBlib.h>

#ifndef XK_MISCELLANY
#define XK_MISCELLANY
#include <rfb/keysymdef.h>
#endif
#include <rfb/LogWriter.h>

#include "i18n.h"
#include "x11.h"
#include "XInputTouchHandler.h"

static rfb::LogWriter vlog("XInputTouchHandler");

XInputTouchHandler::XInputTouchHandler(xcb_window_t wnd_,
                                       GestureCallback* gestureCallback)
  : wnd(wnd_), gestureHandler(gestureCallback)
{
  Display* display;
  XIEventMask *curmasks;
  int num_masks;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  display = QX11Info::display();
#else
  display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  curmasks = XIGetSelectedEvents(display, wnd, &num_masks);
  if (curmasks == nullptr)
    return;

  assert(num_masks == 1);

  // Event delivery is broken when somebody else does a pointer grab,
  // so we need to listen to all devices and do filtering of master
  // devices manually
  assert(curmasks[0].deviceid == XIAllDevices);

  XISetMask(curmasks[0].mask, XI_TouchBegin);
  XISetMask(curmasks[0].mask, XI_TouchUpdate);
  XISetMask(curmasks[0].mask, XI_TouchEnd);

  // If something has a passive grab of touches (e.g. the window
  // manager wants to have its own gestures) then we won't get the
  // touch events until everyone who has a grab has indicated they
  // don't want these touches (via XIAllowTouchEvents()).
  // Unfortunately the touches are then replayed one touch point at
  // a time, meaning things will be delayed and out of order,
  // completely screwing up our gesture detection. Listening for
  // XI_TouchOwnership has the effect of giving us the touch events
  // right away, even if grabbing clients are also getting them.
  //
  // FIXME: We should really wait for the XI_TouchOwnership event
  //        before it is safe to react to the gesture, otherwise we
  //        might react to something that the window manager will
  //        also react to.
  //
  XISetMask(curmasks[0].mask, XI_TouchOwnership);

  XISelectEvents(display, wnd, curmasks, 1);

  XFree(curmasks);
}

bool XInputTouchHandler::handleEvent(const char* eventType,
                                     void* message)
{
  const xcb_generic_event_t* xcbevent;

  assert(eventType);
  assert(message);

  if (strcmp(eventType, "xcb_generic_event_t") != 0)
    return false;

  xcbevent = (xcb_generic_event_t*)message;

  if (xcbevent->response_type == XCB_GE_GENERIC) {
    const xcb_ge_generic_event_t* xcbgeneric;

    xcbgeneric = (xcb_ge_generic_event_t*)xcbevent;

    if (xcbgeneric->extension == x11_xinput_major()) {
      xcb_input_touch_begin_event_t* devev;

      devev = (xcb_input_touch_begin_event_t*)xcbgeneric;

      if (devev->event != wnd)
        return false;

      switch (devev->event_type) {
      case XI_TouchBegin:
      case XI_TouchUpdate:
      case XI_TouchEnd:
      case XI_TouchOwnership:
        break;
      default:
        return false;
      }

      processEvent(xcbgeneric);

      return true;
    }
  }

  return false;
}

void XInputTouchHandler::processEvent(const xcb_ge_generic_event_t* xcbgeneric)
{
  Display* display;
  xcb_input_touch_begin_event_t* devev;

  devev = (xcb_input_touch_begin_event_t*)xcbgeneric;

  bool isMaster = devev->deviceid != devev->sourceid;

  // We're only interested in events from master devices
  if (!isMaster) {
    // However we need to accept TouchEnd from slave devices as they
    // might not get delivered if there is a pointer grab, see:
    // https://gitlab.freedesktop.org/xorg/xserver/-/issues/1016
    if (devev->event_type != XI_TouchEnd)
      return;
  }

  // Avoid duplicate TouchEnd events, see above
  // FIXME: Doesn't handle floating slave devices
  if (isMaster && devev->event_type == XI_TouchEnd)
    return;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  display = QX11Info::display();
#else
  display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  switch (devev->event_type) {
  case XI_TouchBegin:
    // XInput2 wants us to explicitly accept touch sequences
    // for grabbed devices before it will pass events
    // FIXME: Should we call this when not grabbing? Qt doesn't
    XIAllowTouchEvents(display,
                       devev->deviceid,
                       devev->detail,
                       devev->event,
                       XIAcceptTouch);

    gestureHandler.handleTouchBegin(devev->detail, devev->event_x, devev->event_y);
    break;
  case XI_TouchUpdate:
    gestureHandler.handleTouchUpdate(devev->detail, devev->event_x, devev->event_y);
    break;
  case XI_TouchEnd:
    gestureHandler.handleTouchEnd(devev->detail);
    break;
  case XI_TouchOwnership:
    // FIXME: Currently ignored, see constructor
    break;
  }
}
