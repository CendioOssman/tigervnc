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

#include <vector>

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include <rfb/Timer.h>

// Internal state bitmasks
#define GH_NOGESTURE   0
#define GH_LEFTBTN     1
#define GH_MIDDLEBTN   2
#define GH_RIGHTBTN    4
#define GH_VSCROLL     8
#define GH_HSCROLL     16
#define GH_ZOOM        32
#define GH_UNDEFINED   (64 | 128)

#define GH_INITSTATE   (255 & ~GH_UNDEFINED)

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
  int id;
  double first_x;
  double first_y;
  double prev_x;
  double prev_y;
  double last_x;
  double last_y;
};

class GestureHandler : public rfb::Timer::Callback {
  public:
    GestureHandler();
    virtual ~GestureHandler();

   void registerEvent(const XIDeviceEvent *ev);
   bool hasDetectedGesture();

   void resetState();

   int sttTimeout();
   int pushEvent(GHEventType t);

  protected:
    virtual void handleGestureEvent(const GHEvent& event) = 0;

  private:
    unsigned char state;

    std::vector<GHTouch> tracked;

    rfb::Timer timeoutTimer;

    virtual bool handleTimeout(rfb::Timer* t);

    int updateTouch(const XIDeviceEvent *ev);
    int trackTouch(const XIDeviceEvent *ev);
    int idxTracked(const XIDeviceEvent *ev);

    size_t avgTrackedTouches(double *x, double *y, GHEventType t);

    int sttTouchEnd();
    int sttTouchUpdate();

    int vDistanceMoved();
    int hDistanceMoved();
    int relDistanceMoved();
};

#endif // __GESTUREHANDLER_H__
