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

#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput2.h>

#include "GestureHandler.h"

static rfb::LogWriter vlog("GestureHandler");

GestureHandler::GestureHandler() : state(GH_INITSTATE), tracked(), eventQueue() {
}

int GestureHandler::registerEvent(void *ev) {
#if !defined(WIN32) && !defined(__APPLE__)
  XIDeviceEvent *devev = (XIDeviceEvent*)ev;

  switch (devev->evtype) {
    case XI_TouchBegin:
      vlog.info("GestureHandler::registerEvent() got XI_TouchBegin");
      // Ignore any new touches if there is already an active gesture
      if (!hasState())
        trackTouch(devev);
      break;

    case XI_TouchUpdate:
      //vlog.info("GestureHandler::registerEvent() got XI_TouchUpdate");
      // FIXME: Maybe only do this if we're in a state??
      //        Or if the movement is above the threshold????
      updateTouch(devev);
      break;

    case XI_TouchEnd:
      vlog.info("GestureHandler::registerEvent() got XI_TouchEnd");
      if (idxTracked(devev) < 0)
        return 0;

      sttTouchEnd();

      // Ending a tracked touch also ends the associated gesture
      pushEvent(GH_GestureEnd);
      resetState();
      break;
  }
#endif
  return tracked.size();
}

int GestureHandler::sttTouchUpdate() {
  if (hasState())
    return this->state;

  // Because it's impossible to distinguish from a scroll, right
  // click can never be initiated by a movement-based trigger.
  this->state &= ~GH_RIGHTBTN;

  switch (tracked.size()) {
    case 0:
      // huh?
      break;

    case 1:
      this->state &= ~(GH_MIDDLEBTN | GH_RIGHTBTN | GH_VSCROLL | GH_ZOOM);
      break;

    case 2:
      this->state &= ~GH_MIDDLEBTN;

     /*
      * - If the finger has moved along the y axis _more_ than what it has
      *   relative to the other finger, then we're not looking at a zoom.
      *
      * - If the finger has moved relative to the other finger _more_ than
      *   what it has along the y axis, then we're not looking at a scroll.
      */
      if (std::abs(relDistanceMoved()) >= std::abs(vDistanceMoved()))
        this->state &= ~GH_VSCROLL;
      else
        this->state &= ~GH_ZOOM;
      break;
  }

  if (hasState()) {
    vlog.info("sttTouchUpdate gave us state %i", this->state);
    pushEvent(GH_GestureBegin);
  }

  return this->state;
}

int GestureHandler::pushEvent(GHEventType t) {
  GHEvent ghev;
  double avg_x, avg_y;

  switch (t) {
    case GH_GestureBegin:
    case GH_GestureEnd:
      avgTrackedTouches(&avg_x, &avg_y, t);
      ghev.detail = this->state;
      ghev.event_x = avg_x;
      ghev.event_y = avg_y;
      break;

    case GH_GestureUpdate:
      if (this->state == GH_VSCROLL || this->state == GH_ZOOM) {
	// For zoom and scroll, we always want the event coordinates
	// to be where the gesture began. So call avgTrackedTouches
	// with GH_GestureBegin instead of GH_GestureUpdate. Also,
	// the detail field for these updates is the magnitude of
	// the update rather than the state (the state is obvious).
        avgTrackedTouches(&avg_x, &avg_y, GH_GestureBegin);
        if (this->state == GH_VSCROLL) {
          ghev.detail = vDistanceMoved();
	  if (std::abs(ghev.detail) < GH_SCRLSENS)
            return 0;
	}
	else if (this->state == GH_ZOOM) {
          ghev.detail = relDistanceMoved();
	  if (std::abs(ghev.detail) < GH_ZOOMSENS)
            return 0;
	}
      }
      else {
        avgTrackedTouches(&avg_x, &avg_y, t);
	ghev.detail = this->state;
      }
      ghev.event_x = avg_x;
      ghev.event_y = avg_y;
      break;
  }

  ghev.type = t;
  eventQueue.push_back(ghev);

  return 1;
}

int GestureHandler::relDistanceMoved() {
  int avg_dist = 0;

  if (tracked.size() < 2)
    return 0;

  for (size_t i = 1; i < tracked.size(); i++) {
    int dx_t0 = tracked[i].prev_x - tracked[i-1].prev_x;
    int dy_t0 = tracked[i].prev_y - tracked[i-1].prev_y;
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
    avg_dist += tracked[i].prev_y - tracked[i].last_y;
  }

  avg_dist /= tracked.size();

  return GH_INVRTSCRL ? -avg_dist : avg_dist;
}

int GestureHandler::sttTimeout() {
  if (hasState())
    return this->state;

  // Scroll and zoom are no longer valid gestures
  this->state &= ~(GH_VSCROLL | GH_ZOOM);

  switch (tracked.size()) {
    case 0:
      this->state = GH_INITSTATE;
      break;

    case 1:
      // Not a multi-touch event
#if (GH_STLPMODE == 1)
      this->state &= ~(GH_MIDDLEBTN | GH_RIGHTBTN);
#else
      this->state &= ~(GH_LEFTBTN | GH_MIDDLEBTN);
#endif
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

  vlog.info("State is %i, size = %li", this->state, tracked.size());

  if (hasState())
#if (GH_DTLPMODE == 1)
    pushEvent(GH_GestureBegin);
#else
    if (!(tracked.size() == 2 && this->state == GH_RIGHTBTN))
      pushEvent(GH_GestureBegin);
#endif

  return this->state;
}

int GestureHandler::sttTouchEnd() {
// FIXME: This is where we need to figure out the logic interplay
//        between GH_DTLMODE and GH_STLPMODE. For now, it doesn't
//        quite work properly.
#if (GH_DTLPMODE == 1)
  if (hasState())
#else
  if (hasState() && this->state != GH_RIGHTBTN)
#endif
    return this->state;

  // Scroll and zoom are no longer valid gestures
  this->state &= ~(GH_VSCROLL | GH_ZOOM);

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

  if (hasState())
    pushEvent(GH_GestureBegin);

  return this->state;
}

void GestureHandler::resetState() {
  this->state = GH_INITSTATE;
  tracked.clear();
}

unsigned char GestureHandler::getState() {
  return this->state;
}

bool GestureHandler::hasState() {
  // Invalid state if any of the undefined bits are set
  if ((state & GH_UNDEFINED) != 0) {
    return False;
  }

  // Check to see if the bitmask value is a power of 2
  // (i.e. only one bit set). If it is, we have a state.
  return state && !(state & (state - 1));
}

int GestureHandler::idxTracked(XIDeviceEvent *ev) {
  for (size_t i = 0; i < tracked.size(); i++) {
    if (tracked[i].id == ev->detail)
      return (int)i;
  }

  return -1;
}

int GestureHandler::trackTouch(XIDeviceEvent *ev) {
  GHTouch ght;

  // FIXME: Perhaps implement some sanity checks here,
  // e.g. duplicate IDs etc
  
  ght.id = ev->detail;
  ght.first_x = ev->event_x;
  ght.first_y = ev->event_y;
  ght.prev_x = ght.first_x;
  ght.prev_y = ght.first_y;
  ght.last_x = ght.prev_x;
  ght.last_y = ght.prev_y;

  tracked.push_back(ght);

  switch (tracked.size()) {
    case 1:
      break;

    case 2:
      this->state &= ~GH_LEFTBTN;
      break;

    case 3:
      this->state &= ~(GH_RIGHTBTN | GH_VSCROLL | GH_ZOOM);
      break;

    default:
      this->state = GH_NOGESTURE;
  }

  if (hasState())
    pushEvent(GH_GestureBegin);

  return tracked.size();
}

std::vector<GHEvent> GestureHandler::getEventQueue() {
  return eventQueue;
}

void GestureHandler::clearEventQueue() {
  eventQueue.clear();
}

size_t GestureHandler::avgTrackedTouches(double *x, double *y, GHEventType t) {
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

  return size;
}

int GestureHandler::updateTouch(XIDeviceEvent *ev) {
  int idx = idxTracked(ev);

  // If this is an update for a touch we're not tracking, ignore it
  if (idx < 0)
    return 0;

  // If the move is smaller than the minimum threshold, ignore it
  if (std::abs(tracked[idx].first_x - ev->event_x) < GH_MTHRESHOLD &&
      std::abs(tracked[idx].first_y - ev->event_y) < GH_MTHRESHOLD)
    return 0;

  // Update the touches last position with the event coordinates
  tracked[idx].last_x = ev->event_x;
  tracked[idx].last_y = ev->event_y;

  sttTouchUpdate();

  if (pushEvent(GH_GestureUpdate)) {
    // By only doing this update on a successful GestureUpdate, we ensure
    // that thresholds are treated as cumulative; i.e. a 30px threshold
    // will be met after any number of updates total to 30. The alternative
    // would be per-update thresholds, in which case gestures would respond
    // to speed of change rather than total distance.
    tracked[idx].prev_x = tracked[idx].last_x;
    tracked[idx].prev_y = tracked[idx].last_y;
  }

  return idx;
}
