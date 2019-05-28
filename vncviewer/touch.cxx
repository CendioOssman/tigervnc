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
#include <X11/XKBlib.h>
#endif

#include <FL/Fl.H>
#include <FL/x.H>

#include <rfb/LogWriter.h>

#if !defined(WIN32) && !defined(__APPLE__)
#include "GestureHandler.h"
#endif // !defined(WIN32) && !defined(__APPLE__)

#include "vncviewer.h"
#include "i18n.h"
#include "touch.h"

static rfb::LogWriter vlog("Touch");

#if !defined(WIN32) && !defined(__APPLE__)
static int xi_major;
#endif

#if !defined(WIN32) && !defined(__APPLE__)

// Sensitivity threshold for gestures
    // By only doing this update on a successful GestureUpdate, we ensure
    // that thresholds are treated as cumulative; i.e. a 30px threshold
    // will be met after any number of updates total to 30. The alternative
    // would be per-update thresholds, in which case gestures would respond
    // to speed of change rather than total distance.
#define GH_ZOOMSENS    30
#define GH_SCRLSENS    50

class FLTKGestureHandler: public GestureHandler
{
public:
  FLTKGestureHandler(Window wnd);

  void processEvent(const XIDeviceEvent* devev);

protected:
  void copyXEventFields(XEvent* dst, const XIDeviceEvent* src);
  void setTouchEventState(XEvent* dst);
  void fakeMotionEvent(const XIDeviceEvent* origEvent);
  void fakeButtonEvent(bool press, int button,
                       const XIDeviceEvent* origEvent);

  void prepareTouchXEvent(XEvent* dst, int x, int y);
  void fakeMotionEvent(int x, int y);
  void fakeButtonEvent(bool press, int button, int x, int y);

  virtual void handleGestureEvent(const GHEvent& event);


private:
  Window wnd;
  int last_value;
  int touchButtonMask;
};

static std::map<Window, class FLTKGestureHandler*> handlers;

FLTKGestureHandler::FLTKGestureHandler(Window wnd)
  : wnd(wnd), touchButtonMask(0)
{
}

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

void FLTKGestureHandler::prepareTouchXEvent(XEvent* dst, int x, int y)
{
  Window root, child;
  int x_root, y_root;
  XkbStateRec state;

  // We don't have a real event to steal things from, so we'll have
  // to fake these events based on the current state of things

  root = XDefaultRootWindow(fl_display);
  XTranslateCoordinates(fl_display, wnd, root, x, y, &x_root, &y_root, &child);
  XkbGetState(fl_display, XkbUseCoreKbd, &state);

  // XButtonEvent and XMotionEvent are almost identical, so we
  // don't have to care which it is for these fields
  dst->xbutton.serial = XLastKnownRequestProcessed(fl_display);
  dst->xbutton.display = fl_display;
  dst->xbutton.window = wnd;
  dst->xbutton.root = root;
  dst->xbutton.subwindow = None;
  dst->xbutton.time = fl_event_time;
  dst->xbutton.x = x;
  dst->xbutton.y = y;
  dst->xbutton.x_root = x_root;
  dst->xbutton.y_root = y_root;
  dst->xbutton.state = state.mods;
  dst->xbutton.state |= ((state.ptr_buttons >> 1) & 0x1f) << 8; // FIXME: Check this!
  dst->xbutton.same_screen = True; // FIXME
}

void FLTKGestureHandler::fakeMotionEvent(int x, int y)
{
  XEvent fakeEvent;

  memset(&fakeEvent, 0, sizeof(XEvent));

  fakeEvent.type = MotionNotify;
  fakeEvent.xmotion.is_hint = False;
  prepareTouchXEvent(&fakeEvent, x, y);
  setTouchEventState(&fakeEvent);

  fl_handle(fakeEvent);
}

void FLTKGestureHandler::fakeButtonEvent(bool press, int button, int x, int y)
{
  XEvent fakeEvent;

  if (press)
    touchButtonMask |= 1 << button; // FIXME: Check this
  else
    touchButtonMask &= ~(1 << button);

  memset(&fakeEvent, 0, sizeof(XEvent));

  fakeEvent.type = press ? ButtonPress : ButtonRelease;
  fakeEvent.xbutton.button = button;
  prepareTouchXEvent(&fakeEvent, x, y);
  setTouchEventState(&fakeEvent);

  fl_handle(fakeEvent);
}

void FLTKGestureHandler::handleGestureEvent(const GHEvent& ev)
{
  switch (ev.type) {
  case GH_GestureBegin:
    switch (ev.gesture) {
    case GH_ONETAP:
      vlog.info("Got GH_GestureBegin(GH_ONETAP)");
      fakeMotionEvent(ev.event_x, ev.event_y);
      fakeButtonEvent(true, Button1, ev.event_x, ev.event_y);
      fakeButtonEvent(false, Button1, ev.event_x, ev.event_y);
      break;
    case GH_TWOTAP:
      vlog.info("Got GH_GestureBegin(GH_TWOTAP)");
      break;
    case GH_THREETAP:
      vlog.info("Got GH_GestureBegin(GH_THREETAP)");
      break;
    case GH_DRAG:
      vlog.info("Got GH_GestureBegin(GH_DRAG)");
      break;
    case GH_LONGPRESS:
      vlog.info("Got GH_GestureBegin(GH_LONGPRESS)");
      break;
    case GH_VSCROLL:
      vlog.info("Got GH_GestureBegin(GH_VSCROLL)");
      last_value = 0;
      if (abs(ev.detail - last_value) < GH_SCRLSENS)
        break;
      last_value = ev.detail;
      break;
    case GH_ZOOM:
      vlog.info("Got GH_GestureBegin(GH_ZOOM)");
      last_value = 0;
      if (abs(ev.detail - last_value) < GH_ZOOMSENS)
        break;
      last_value = ev.detail;
      break;
    }
    break;

  case GH_GestureUpdate:
    switch (ev.gesture) {
    case GH_ONETAP:
      vlog.info("Got GH_GestureUpdate(GH_ONETAP)");
      break;
    case GH_TWOTAP:
      vlog.info("Got GH_GestureUpdate(GH_TWOTAP)");
      break;
    case GH_THREETAP:
      vlog.info("Got GH_GestureUpdate(GH_THREETAP)");
      break;
    case GH_DRAG:
      vlog.info("Got GH_GestureUpdate(GH_DRAG)");
      break;
    case GH_LONGPRESS:
      vlog.info("Got GH_GestureUpdate(GH_LONGPRESS)");
      break;
    case GH_VSCROLL:
      vlog.info("Got GH_GestureUpdate(GH_VSCROLL)");
      if (abs(ev.detail - last_value) < GH_SCRLSENS)
        break;
      last_value = ev.detail;
      break;
    case GH_HSCROLL:
      vlog.info("Got GH_GestureUpdate(GH_HSCROLL)");
      if (abs(ev.detail - last_value) < GH_SCRLSENS)
        break;
      last_value = ev.detail;
      break;
    case GH_ZOOM:
      vlog.info("Got GH_GestureUpdate(GH_ZOOM)");
      if (abs(ev.detail - last_value) < GH_ZOOMSENS)
        break;
      last_value = ev.detail;
      break;
    }
    break;

  case GH_GestureEnd:
    switch (ev.gesture) {
    case GH_ONETAP:
      vlog.info("Got GH_GestureEnd(GH_ONETAP)");
      break;
    case GH_TWOTAP:
      vlog.info("Got GH_GestureEnd(GH_TWOTAP)");
      break;
    case GH_THREETAP:
      vlog.info("Got GH_GestureEnd(GH_THREETAP)");
      break;
    case GH_DRAG:
      vlog.info("Got GH_GestureEnd(GH_DRAG)");
      break;
    case GH_LONGPRESS:
      vlog.info("Got GH_GestureEnd(GH_LONGPRESS)");
      break;
    case GH_VSCROLL:
      vlog.info("Got GH_GestureEnd(GH_VSCROLL)");
      break;
    case GH_HSCROLL:
      vlog.info("Got GH_GestureEnd(GH_HSCROLL)");
      break;
    case GH_ZOOM:
      vlog.info("Got GH_GestureEnd(GH_ZOOM)");
      break;
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

    handlers[xevent->xmap.window] = new FLTKGestureHandler(xevent->xmap.window);

    // Fall through as we don't want to interfere with whatever someone
    // else might want to do with this event

  } else if (xevent->type == UnmapNotify) {
    handlers.erase(xevent->xunmap.window);
  } else if (xevent->type == DestroyNotify) {
    handlers.erase(xevent->xdestroywindow.window);
  } else if (xevent->type == GenericEvent) {
    if (xevent->xgeneric.extension == xi_major) {
      XIDeviceEvent *devev;

      if (!XGetEventData(fl_display, &xevent->xcookie)) {
        vlog.error(_("Failed to get event data for X Input event"));
        return 1;
      }

      devev = (XIDeviceEvent*)xevent->xcookie.data;

      if (handlers.count(devev->event) == 0) {
        vlog.error(_("X Input event for unknown window"));
        XFreeEventData(fl_display, &xevent->xcookie);
        return 1;
      }

      // FLTK doesn't understand X Input events, and we've stopped
      // delivery of Core events by enabling the X Input ones. Make
      // FLTK happy by faking Core events based on the X Input ones.

      handlers[devev->event]->processEvent(devev);

      XFreeEventData(fl_display, &xevent->xcookie);

      return 1;
    }
  }

  return 0;
}

XEvent fakeXEvent(XIDeviceEvent *devev) {
  XEvent fakeEvent;

  switch (devev->evtype) {
    case XI_Motion:
      fakeEvent.type = MotionNotify;
      fakeEvent.xmotion.is_hint = False;
      break;
    case XI_ButtonPress:
      fakeEvent.type = ButtonPress;
      fakeEvent.xbutton.button = devev->detail;
      break;
   case XI_ButtonRelease:
      fakeEvent.type = ButtonRelease;
      fakeEvent.xbutton.button = devev->detail;
      break;
  }

  // XButtonEvent and XMotionEvent are almost identical, so we
  // don't have to care which it is for these fields
  fakeEvent.xbutton.serial = devev->serial;
  fakeEvent.xbutton.display = devev->display;
  fakeEvent.xbutton.window = devev->event;
  fakeEvent.xbutton.root = devev->root;
  fakeEvent.xbutton.subwindow = devev->child;
  fakeEvent.xbutton.time = devev->time;
  fakeEvent.xbutton.x = devev->event_x;
  fakeEvent.xbutton.y = devev->event_y;
  fakeEvent.xbutton.x_root = devev->root_x;
  fakeEvent.xbutton.y_root = devev->root_y;
  fakeEvent.xbutton.state = devev->mods.effective;
  fakeEvent.xbutton.state |= ((devev->buttons.mask[0] >> 1) & 0x1f) << 8;
  fakeEvent.xbutton.same_screen = True; // FIXME

  return fakeEvent;
}
#endif

void enable_touch()
{
#if !defined(WIN32) && !defined(__APPLE__)
  int ev, err;
  int major_ver, minor_ver;

  fl_open_display();

  if (!XQueryExtension(fl_display, "XInputExtension", &xi_major, &ev, &err)) {
    exit_vncviewer(_("X Input extension not available."));
    return; // Not reached
  }

  major_ver = 2;
  minor_ver = 2;
  if (XIQueryVersion(fl_display, &major_ver, &minor_ver) != Success) {
    exit_vncviewer(_("X Input 2 (or newer) is not available."));
    return; // Not reached
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

