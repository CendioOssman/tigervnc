/* Copyright 2019 Aaron Sowry for Cendio AB
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

#ifndef __GESTUREHANDLER_H__
#define __GESTUREHANDLER_H__

#include <map>
#include <set>

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include <rfb/Timer.h>

// Internal state bitmasks
#define GH_NOGESTURE   0
#define GH_ONETAP      1
#define GH_TWOTAP      2
#define GH_THREETAP    4
#define GH_DRAG        8
#define GH_LONGPRESS   16
#define GH_VSCROLL     32
#define GH_HSCROLL     64
#define GH_ZOOM        128

#define GH_INITSTATE   255

enum GHEventType {
  GH_GestureBegin,
  GH_GestureUpdate,
  GH_GestureEnd,
};

struct GHEvent {
  unsigned char gesture;
  int detail;
  double event_x;
  double event_y;
  GHEventType type;
};

struct GHTouch {
  struct timeval started;
  double first_x;
  double first_y;
  double last_x;
  double last_y;
};

class GestureHandler : public rfb::Timer::Callback {
  public:
    GestureHandler();
    virtual ~GestureHandler();

   void registerEvent(const XIDeviceEvent *ev);

  protected:
    virtual void handleGestureEvent(const GHEvent& event) = 0;

  private:
    unsigned char state;

    std::map<int, GHTouch> tracked;
    std::set<int> ignored;

    rfb::Timer longpressTimer;

   bool hasDetectedGesture();

   void resetState();

   void pushEvent(GHEventType t);

   void longpressTimeout();
    virtual bool handleTimeout(rfb::Timer* t);

    void updateTouch(const XIDeviceEvent *ev);
    void trackTouch(const XIDeviceEvent *ev);
    void endTouch(const XIDeviceEvent *ev);
    void endGesture();

    void avgTrackedTouches(double *first_x, double *first_y, double *last_x, double *last_y);

    int vDistanceMoved();
    int hDistanceMoved();
    int relDistanceMoved();
};

#endif // __GESTUREHANDLER_H__
