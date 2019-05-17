/* Copyright 2019 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright 2019 Aaron Sowry for Cendio AB
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

#if ! (defined(WIN32) || defined(__APPLE__))
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2.h>
#endif

#include <FL/Fl.H>
#include <FL/x.H>

#include <rfb/LogWriter.h>

#if !defined(WIN32) && !defined(__APPLE__)
#include "GestureHandler.h"
#endif // !defined(WIN32) && !defined(__APPLE__)

#include "touch.h"

static rfb::LogWriter vlog("Touch");

#if !defined(WIN32) && !defined(__APPLE__)
static int xi_major;
#endif

#if !defined(WIN32) && !defined(__APPLE__)

class FLTKGestureHandler: public GestureHandler
{
public:
  void processEvent(const XIDeviceEvent* devev);

protected:
  void copyXEventFields(XEvent* dst, const XIDeviceEvent* src);
  void setTouchEventState(XEvent* dst);
  void fakeMotionEvent(const XIDeviceEvent* origEvent);
  void fakeButtonEvent(bool press, int button,
                       const XIDeviceEvent* origEvent);

  virtual void handleGestureEvent(const GHEvent& event);

private:
  int touchButtonMask;
};

static class FLTKGestureHandler gh;

void FLTKGestureHandler::processEvent(const XIDeviceEvent* devev)
{
  switch (devev->evtype) {
  case XI_Motion:
    fakeMotionEvent(devev);
    break;
  case XI_ButtonPress:
    fakeButtonEvent(true, devev->detail, devev);
    break;
  case XI_ButtonRelease:
    fakeButtonEvent(false, devev->detail, devev);
    break;
  case XI_TouchBegin:
  case XI_TouchUpdate:
  case XI_TouchEnd:
    registerEvent(devev);
    break;
  }
}

void FLTKGestureHandler::copyXEventFields(XEvent* dst,
                                          const XIDeviceEvent* src)
{
  // XButtonEvent and XMotionEvent are almost identical, so we
  // don't have to care which it is for these fields
  dst->xbutton.serial = src->serial;
  dst->xbutton.display = src->display;
  dst->xbutton.window = src->event;
  dst->xbutton.root = src->root;
  dst->xbutton.subwindow = src->child;
  dst->xbutton.time = src->time;
  dst->xbutton.x = src->event_x;
  dst->xbutton.y = src->event_y;
  dst->xbutton.x_root = src->root_x;
  dst->xbutton.y_root = src->root_y;
  dst->xbutton.state = src->mods.effective;
  dst->xbutton.state |= ((src->buttons.mask[0] >> 1) & 0x1f) << 8;
  dst->xbutton.same_screen = True; // FIXME
}

void FLTKGestureHandler::setTouchEventState(XEvent* dst)
{
  if (tracking_touch)
    dst->xbutton.state |= touchButtonMask << 8;
}

void FLTKGestureHandler::fakeMotionEvent(const XIDeviceEvent* origEvent)
{
  XEvent fakeEvent;

  memset(&fakeEvent, 0, sizeof(XEvent));

  fakeEvent.type = MotionNotify;
  fakeEvent.xmotion.is_hint = False;
  copyXEventFields(&fakeEvent, origEvent);
  setTouchEventState(&fakeEvent);

  fl_handle(fakeEvent);
}

void FLTKGestureHandler::fakeButtonEvent(bool press, int button,
                                         const XIDeviceEvent* origEvent)
{
  XEvent fakeEvent;

  memset(&fakeEvent, 0, sizeof(XEvent));

  fakeEvent.type = press ? ButtonPress : ButtonRelease;
  fakeEvent.xbutton.button = button;
  copyXEventFields(&fakeEvent, origEvent);
  setTouchEventState(&fakeEvent);

  fl_handle(fakeEvent);
}

void FLTKGestureHandler::handleGestureEvent(const GHEvent& ev)
{
  // FIXME: Can we get away with clobbering buttonMask like this
  // in the case of touch gestures, rather than bitshifting?
  switch (ev.type) {
  case GH_GestureBegin:
    vlog.info("GH_GestureBegin (%i)", ev.detail);
    switch (ev.detail) {
    case GH_LEFTBTN:
    case GH_MIDDLEBTN:
    case GH_RIGHTBTN:
      fakeMotionEvent(ev);
      fakeButtonEvent(true, ev.detail, ev);
      break;

/*    case GH_ZOOM:
      self->handleKeyPress(0x1d, XK_Control_L);
      break;*/
    }
    break;

  case GH_GestureUpdate:
    vlog.info("GH_GestureUpdate (%i)", ev.detail);
    switch (gh->getState()) {
    case GH_LEFTBTN:
    case GH_MIDDLEBTN:
    case GH_RIGHTBTN:
      fakeMotionEvent(ev);
      break;
/*
    case GH_VSCROLL:
    case GH_ZOOM:
      if (ev.detail > 0)
        self->handlePointerEvent(rfb::Point(ev.event_x, ev.event_y), 8);
      else if (ev.detail < 0)
        self->handlePointerEvent(rfb::Point(ev.event_x, ev.event_y), 16);
      break;*/
    }
    break;

  case GH_GestureEnd:
    vlog.info("GH_GestureEnd (%i)", ev.detail);
    switch (ev.detail) {
    case GH_LEFTBTN:
    case GH_MIDDLEBTN:
    case GH_RIGHTBTN:
      fakeMotionEvent(ev);
      fakeButtonEvent(false, ev.detail, ev);
      break;
/*
    case GH_ZOOM:
      self->handleKeyRelease(0x1d);
      break;*/
    }
    break;
  }
}

static int handleXinputEvent(void *event, void *data)
{
  XEvent *xevent = (XEvent*)event;

  if (xevent->type == MapNotify) {
    XIEventMask eventmask;
    unsigned char flags[XIMaskLen(XI_LASTEVENT)] = { 0 };

    eventmask.deviceid = XIAllMasterDevices;
    eventmask.mask_len = sizeof(flags);
    eventmask.mask = flags;

    XISetMask(flags, XI_ButtonPress);
    XISetMask(flags, XI_Motion);
    XISetMask(flags, XI_ButtonRelease);

    XISetMask(flags, XI_TouchBegin);
    XISetMask(flags, XI_TouchUpdate);
    XISetMask(flags, XI_TouchEnd);

    XISelectEvents(fl_display, xevent->xmap.window, &eventmask, 1);

    // Fall through as we don't want to interfere with whatever someone
    // else might want to do with this event

  } else if (xevent->type == GenericEvent) {
    if (xevent->xgeneric.extension == xi_major) {
      XIDeviceEvent *devev;

      if (!XGetEventData(fl_display, &xevent->xcookie)) {
        vlog.error("Failed to get event data for X Input event");
        return 1;
      }

      devev = (XIDeviceEvent*)xevent->xcookie.data;

      // FLTK doesn't understand X Input events, and we've stopped
      // delivery of Core events by enabling the X Input ones. Make
      // FLTK happy by faking Core events based on the X Input ones.

      gh.processEvent(devev);

      XFreeEventData(fl_display, &xevent->xcookie);

      return 1;
    }
  }

  return 0;
}
#endif

void enable_touch()
{
#if !defined(WIN32) && !defined(__APPLE__)
  int ev, err;
  int major_ver, minor_ver;

  fl_open_display();

  if (!XQueryExtension(fl_display, "XInputExtension", &xi_major, &ev, &err)) {
    vlog.error("X Input extension not available.");
    // FIXME: fatal
    return;
  }

  major_ver = 2;
  minor_ver = 2;
  if (XIQueryVersion(fl_display, &major_ver, &minor_ver) != Success) {
    vlog.error("X Input 2 (or newer) is not available.");
    // FIXME: fatal
    return;
  }

  if ((major_ver == 2) && (minor_ver < 2))
    vlog.error("X Input 2.2 (or newer) is not available. Touch gestures will not be supported.");

  Fl::add_system_handler(handleXinputEvent, NULL);
#endif
}

void disable_touch()
{
#if !defined(WIN32) && !defined(__APPLE__)
  Fl::remove_system_handler(handleXinputEvent);
#endif
}
