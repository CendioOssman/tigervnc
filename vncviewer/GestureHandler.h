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

// Internal state bitmasks
#define GH_NOGESTURE   0
#define GH_LEFTBTN     1
#define GH_MIDDLEBTN   2
#define GH_RIGHTBTN    4
#define GH_VSCROLL     8
#define GH_ZOOM        16
#define GH_UNDEFINED   (32 | 64 | 128)

#define GH_INITSTATE   (255 & ~GH_UNDEFINED)

// Movement threshold for gestures
#define GH_MTHRESHOLD  50

// Sensitivity threshold for gestures
#define GH_ZOOMSENS    30
#define GH_SCRLSENS    50

// Invert the scroll
#define GH_INVRTSCRL   1

// Enable timeout state transition
// 0 = Disabled
// 1 = Enabled
#define GH_STTIMEOUT   1

// Timeout when waiting for gestures
#define GH_STTDELAY    0.25 // s

// Single-touch long-press mode
// Only valid with GH_STTIMEOUT
// 1 = Left button click-and-hold
// 2 = Right button click-and-hold
#define GH_STLPMODE    2

// Double-touch long-press mode
// Only valid with GH_STTIMEOUT
// 1 = Right button click-and-hold
// 2 = No effect (click on release)
#define GH_DTLPMODE    2

// TODO: A switch for the other STTs
//       (sttTouchUpdate and sttEndTouch)

enum GHEventType {
  GH_GestureBegin,
  GH_GestureUpdate,
  GH_GestureEnd,
};

struct GHEvent {
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

class GestureHandler {
  public:
    GestureHandler();
    ~GestureHandler();

   int registerEvent(void *ev);
   unsigned char getState();
   bool hasState();

   std::vector<GHEvent> getEventQueue();
   void clearEventQueue();

   void resetState();

   int sttTimeout();
   int pushEvent(GHEventType t);

  private:
    unsigned char state;

    std::vector<GHTouch> tracked;
    std::vector<GHEvent> eventQueue;

    int updateTouch(XIDeviceEvent *ev);
    int trackTouch(XIDeviceEvent *ev);
    int idxTracked(XIDeviceEvent *ev);

    size_t avgTrackedTouches(double *x, double *y, GHEventType t);

    int sttTouchEnd();
    int sttTouchUpdate();

    int vDistanceMoved();
    int relDistanceMoved();
};

#endif // __GESTUREHANDLER_H__
