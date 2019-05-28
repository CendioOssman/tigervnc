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

#include <cmath>

#include <rfb/LogWriter.h>
#include <rfb/util.h>

#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput2.h>

#include "GestureHandler.h"

static rfb::LogWriter vlog("GestureHandler");

// Movement threshold for gestures
const unsigned GH_MTHRESHOLD = 50;

// Invert the scroll
#define GH_INVRTSCRL   1

// Enable timeout state transition
// 0 = Disabled
// 1 = Enabled
#define GH_STTIMEOUT   1

// Timeout when waiting for gestures (ms)
const int GH_MULTITOUCH_TIMEOUT = 250;

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

GestureHandler::GestureHandler() :
  state(GH_INITSTATE) {
}

GestureHandler::~GestureHandler()
{
}

void GestureHandler::registerEvent(const XIDeviceEvent *devev) {

  switch (devev->evtype) {
    case XI_TouchBegin:
      // Ignore any new touches if there is already an active gesture
      if (hasDetectedGesture()) {
        ignored.insert(devev->detail);
        break;
      }

      trackTouch(devev);
      break;

    case XI_TouchUpdate:
      // FIXME: Maybe only do this if we're in a state??
      //        Or if the movement is above the threshold????
      updateTouch(devev);
      break;

    case XI_TouchEnd:
      // Something we're tracking?
      if (tracked.count(devev->detail) > 0) {
        endTouch(devev);
      } else {
        ignored.erase(devev->detail);
      }

      if (tracked.empty() && ignored.empty())
        resetState();

      break;
  }
}

void GestureHandler::pushEvent(GHEventType t) {
  GHEvent ghev;
  double avg_x, avg_y;

  switch (t) {
    case GH_GestureBegin:
    case GH_GestureEnd:
      avgTrackedTouches(&avg_x, &avg_y, t);
      ghev.gesture = this->state;
      ghev.event_x = avg_x;
      ghev.event_y = avg_y;
      break;

    case GH_GestureUpdate:
      if (this->state == GH_VSCROLL || this->state == GH_HSCROLL || this->state == GH_ZOOM) {
	// For zoom and scroll, we always want the event coordinates
	// to be where the gesture began. So call avgTrackedTouches
	// with GH_GestureBegin instead of GH_GestureUpdate. Also,
	// the detail field for these updates is the magnitude of
	// the update rather than the state (the state is obvious).
        avgTrackedTouches(&avg_x, &avg_y, GH_GestureBegin);
        if (this->state == GH_VSCROLL)
          ghev.detail = vDistanceMoved();
	else if (this->state == GH_HSCROLL)
          ghev.detail = hDistanceMoved();
	else // GH_ZOOM
          ghev.detail = relDistanceMoved();
      }
      else {
        avgTrackedTouches(&avg_x, &avg_y, t);
      }
      ghev.gesture = this->state;
      ghev.event_x = avg_x;
      ghev.event_y = avg_y;
      break;
  }

  ghev.type = t;

  handleGestureEvent(ghev);
}

int GestureHandler::relDistanceMoved() {
  int avg_dist = 0;

  if (tracked.size() < 2)
    return 0;

  for (size_t i = 1; i < tracked.size(); i++) {
    int dx_t0 = tracked[i].first_x - tracked[i-1].first_x;
    int dy_t0 = tracked[i].first_y - tracked[i-1].first_y;
    int dt0 = std::sqrt(dx_t0 * dx_t0 + dy_t0 * dy_t0);

    int dx_t1 = tracked[i].last_x - tracked[i-1].last_x;
    int dy_t1 = tracked[i].last_y - tracked[i-1].last_y;
    int dt1 = std::sqrt(dx_t1 * dx_t1 + dy_t1 * dy_t1);

    avg_dist += (dt1 - dt0);
  }

  return avg_dist / tracked.size();
}

int GestureHandler::vDistanceMoved() {
  int avg_dist = 0;

  for (size_t i = 0; i < tracked.size(); i++) {
    avg_dist += tracked[i].first_y - tracked[i].last_y;
  }

  avg_dist /= tracked.size();

  return GH_INVRTSCRL ? -avg_dist : avg_dist;
}

int GestureHandler::hDistanceMoved() {
  int avg_dist = 0;

  for (size_t i = 0; i < tracked.size(); i++) {
    avg_dist += tracked[i].first_x - tracked[i].last_x;
  }

  avg_dist /= tracked.size();

  return GH_INVRTSCRL ? -avg_dist : avg_dist;
}

void GestureHandler::resetState() {
  this->state = GH_INITSTATE;
  tracked.clear();
}

bool GestureHandler::hasDetectedGesture() {
  // Invalid state if any of the undefined bits are set
  if ((state & GH_UNDEFINED) != 0) {
    return false;
  }

  // Check to see if the bitmask value is a power of 2
  // (i.e. only one bit set). If it is, we have a state.
  return state && !(state & (state - 1));
}

void GestureHandler::trackTouch(const XIDeviceEvent *ev) {
  GHTouch ght;

  // FIXME: Perhaps implement some sanity checks here,
  // e.g. duplicate IDs etc
  
  gettimeofday(&ght.started, NULL);
  ght.last_x = ght.first_x = ev->event_x;
  ght.last_y = ght.first_y = ev->event_y;

  tracked[ev->detail] = ght;

  // Did it take too long between touches that we should no longer
  // consider this a single gesture?
  if (tracked.size() > 1) {
    std::map<int, GHTouch>::const_iterator iter;
    for (iter = tracked.begin(); iter != tracked.end(); ++iter) {
      if (rfb::msSince(&iter->second.started) > GH_MULTITOUCH_TIMEOUT) {
        this->state = GH_NOGESTURE;
        break;
      }
    }
  }

  switch (tracked.size()) {
    case 1:
      break;

    case 2:
      this->state &= ~GH_LEFTBTN;
      break;

    case 3:
      this->state &= ~(GH_RIGHTBTN | GH_VSCROLL | GH_HSCROLL | GH_ZOOM);
      break;

    default:
      this->state = GH_NOGESTURE;
  }

  if (hasDetectedGesture())
    pushEvent(GH_GestureBegin);
#if (GH_STTIMEOUT)
  else if (tracked.size() == 1)
    timeoutTimer.start(GH_STTDELAY);
#endif
}

void GestureHandler::avgTrackedTouches(double *x, double *y, GHEventType t) {
  size_t size = tracked.size();
  double _x = 0, _y = 0;

  switch (t) {
    case GH_GestureBegin:
      for (size_t i = 0; i < size; i++) {
        _x += tracked[i].first_x;
        _y += tracked[i].first_y;
      }
      break;

    case GH_GestureUpdate:
    case GH_GestureEnd:
      for (size_t i = 0; i < size; i++) {
        _x += tracked[i].last_x;
        _y += tracked[i].last_y;
      }
      break;
  }

  *x = _x / size;
  *y = _y / size;
}

bool GestureHandler::handleTimeout(rfb::Timer* t)
{
  if (t == &timeoutTimer)
    touchTimeout();

  return False;
}

void GestureHandler::updateTouch(const XIDeviceEvent *ev) {
  GHTouch *touch;

  // If this is an update for a touch we're not tracking, ignore it
  if (tracked.count(ev->detail) == 0)
    return;

  touch = &tracked[ev->detail];

  // Update the touches last position with the event coordinates
  touch->last_x = ev->event_x;
  touch->last_y = ev->event_y;

  if (hasDetectedGesture()) {
    pushEvent(GH_GestureUpdate);
    return;
  }

  // If the move is smaller than the minimum threshold, ignore it
  if (std::abs(touch->first_x - ev->event_x) < GH_MTHRESHOLD &&
      std::abs(touch->first_y - ev->event_y) < GH_MTHRESHOLD)
    return;

  // Because it's impossible to distinguish from a scroll, right
  // click can never be initiated by a movement-based trigger.
  this->state &= ~GH_RIGHTBTN;

  switch (tracked.size()) {
    case 0:
      // huh?
      break;

    case 1:
      this->state &= ~(GH_MIDDLEBTN | GH_RIGHTBTN | GH_VSCROLL | GH_HSCROLL | GH_ZOOM);
      break;

    case 2:
      this->state &= ~GH_MIDDLEBTN;

      int dv = std::abs(vDistanceMoved());
      int dh = std::abs(hDistanceMoved());
      int dt = std::abs(relDistanceMoved());

      if (dv < dh || dv < dt)
        this->state &= ~GH_VSCROLL;

      if (dh < dv || dh < dt)
        this->state &= ~GH_HSCROLL;

      if (dt < dv || dt < dh)
        this->state &= ~GH_ZOOM;

      break;
  }

  if (hasDetectedGesture())
    pushEvent(GH_GestureBegin);
}

void GestureHandler::endTouch(const XIDeviceEvent *ev) {
#if (GH_STTIMEOUT)
  timeoutTimer.stop();
#endif

  // Some gesture don't trigger until a finger is released
  if (!hasDetectedGesture()) {
    // Scroll and zoom are no longer valid gestures
    this->state &= ~(GH_VSCROLL | GH_HSCROLL | GH_ZOOM);

    switch (tracked.size()) {
      case 1:
        // Not a multi-touch event
        this->state &= ~(GH_MIDDLEBTN | GH_RIGHTBTN);
        break;

      case 2:
        // Not a single- or triple-touch gesture
        this->state &= ~(GH_LEFTBTN | GH_MIDDLEBTN);
        break;

      case 3:
        // Not a single- or double-touch gesture
        this->state &= ~(GH_LEFTBTN | GH_RIGHTBTN);
        break;

      default:
        this->state = GH_NOGESTURE;
    }

    if (hasDetectedGesture())
      pushEvent(GH_GestureBegin);
  } else {
#if (GH_DTLPMODE == 2)
    if (tracked.size() == 2 && this->state == GH_RIGHTBTN)
      pushEvent(GH_GestureBegin);
#endif
  }

  // Ending a tracked touch also ends the associated gesture
  if (hasDetectedGesture())
    pushEvent(GH_GestureEnd);

  // Ignore any remaining touches until they are ended
  std::map<int, GHTouch>::const_iterator iter;
  for (iter = tracked.begin(); iter != tracked.end(); ++iter) {
    if (iter->first == ev->detail)
      continue;
    ignored.insert(iter->first);
  }
  tracked.clear();

  state = GH_NOGESTURE;
}
