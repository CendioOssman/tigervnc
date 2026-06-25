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

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2.h>
#include <X11/XKBlib.h>

#include <FL/x.H>

#ifndef XK_MISCELLANY
#define XK_MISCELLANY
#include <rfb/keysymdef.h>
#endif
#include <rfb/LogWriter.h>

#include "i18n.h"
#include "x11.h"
#include "XInputTouchHandler.h"

static rfb::LogWriter vlog("XInputTouchHandler");

XInputTouchHandler::XInputTouchHandler(Window wnd_,
                                       GestureCallback* gestureCallback)
  : wnd(wnd_), gestureHandler(gestureCallback)
{
  XIEventMask *curmasks;
  int num_masks;

  curmasks = XIGetSelectedEvents(fl_display, wnd, &num_masks);
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

  XISelectEvents(fl_display, wnd, curmasks, 1);

  XFree(curmasks);
}

bool XInputTouchHandler::handleEvent(const void* event)
{
  XEvent *xevent = (XEvent*)event;

  if (xevent->type == GenericEvent) {
    if (xevent->xgeneric.extension == x11_xinput_major()) {
      XIDeviceEvent *devev;

      if (!XGetEventData(fl_display, &xevent->xcookie)) {
        vlog.error(_("Failed to get event data for X Input event"));
        return true;
      }

      devev = (XIDeviceEvent*)xevent->xcookie.data;

      if (devev->event != wnd) {
        XFreeEventData(fl_display, &xevent->xcookie);
        return false;
      }

      switch (devev->evtype) {
      case XI_TouchBegin:
      case XI_TouchUpdate:
      case XI_TouchEnd:
      case XI_TouchOwnership:
        break;
      default:
        XFreeEventData(fl_display, &xevent->xcookie);
        return 0;
      }

      processEvent(devev);

      XFreeEventData(fl_display, &xevent->xcookie);

      return true;
    }
  }

  return false;
}

void XInputTouchHandler::processEvent(const XIDeviceEvent* devev)
{
  bool isMaster = devev->deviceid != devev->sourceid;

  // We're only interested in events from master devices
  if (!isMaster) {
    // However we need to accept TouchEnd from slave devices as they
    // might not get delivered if there is a pointer grab, see:
    // https://gitlab.freedesktop.org/xorg/xserver/-/issues/1016
    if (devev->evtype != XI_TouchEnd)
      return;
  }

  // Avoid duplicate TouchEnd events, see above
  // FIXME: Doesn't handle floating slave devices
  if (isMaster && devev->evtype == XI_TouchEnd)
    return;

  switch (devev->evtype) {
  case XI_TouchBegin:
    // XInput2 wants us to explicitly accept touch sequences
    // for grabbed devices before it will pass events
    // FIXME: Should we call this when not grabbing? Qt doesn't
    XIAllowTouchEvents(fl_display,
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
