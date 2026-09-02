/* Copyright 2020 Samuel Mannehed for Cendio AB
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

#include <windows.h>
#include <commctrl.h>

#define XK_MISCELLANY
#include <rfb/keysymdef.h>
#include <rfb/Exception.h>
#include <rfb/LogWriter.h>

#include "GestureHandler.h"
#include "i18n.h"
#include "Win32TouchHandler.h"

static rfb::LogWriter vlog("Win32TouchHandler");

static const DWORD MOUSEMOVE_FLAGS = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE |
                                     MOUSEEVENTF_VIRTUALDESK;

static const unsigned SINGLE_PAN_THRESHOLD = 50;

Win32TouchHandler::Win32TouchHandler(HWND hWnd_,
                                     GestureCallback* gestureCallback_)
  : hWnd(hWnd_), gestureCallback(gestureCallback_),
    gesturesConfigured(false), gestureActive(false),
    ignoringGesture(false)
{
  // If window is registered as touch we can not receive gestures,
  // this should not happen
  if (IsTouchWindow(hWnd, nullptr))
    throw rfb::Exception(_("Window is registered for touch instead of gestures"));

  // We will not receive any touch/gesture events if this service
  // isn't running - Logging is enough
  if (!GetSystemMetrics(SM_DIGITIZER))
    vlog.debug("The 'Tablet PC Input' service is required for touch");

  // When we have less than two touch points we won't receive all
  // gesture events - Logging is enough
  int supportedTouches = GetSystemMetrics(SM_MAXIMUMTOUCHES);
  if (supportedTouches < 2)
    vlog.debug("Two touch points required, system currently supports: %d",
               supportedTouches);

  // Add a special hook-in for handling events sent directly to WndProc
  if (!SetWindowSubclass(hWnd, &windowProc, 1, (DWORD_PTR)this)) {
    vlog.error(_("Couldn't attach event handler to window (error 0x%x)"),
                (int)GetLastError());
  }
}

bool Win32TouchHandler::handleEvent(const char* /*eventType*/,
                                    void* /*message*/)
{
  return false;
}

LRESULT CALLBACK Win32TouchHandler::windowProc(HWND hWnd, UINT uMsg,
                                               WPARAM wParam,
                                               LPARAM lParam,
                                               UINT_PTR /*uIdSubclass*/,
                                               DWORD_PTR dwRefData)
{
  bool handled = false;

  if (uMsg == WM_NCDESTROY) {
    RemoveWindowSubclass(hWnd, &windowProc, 1);
  } else {
    handled = ((Win32TouchHandler*)dwRefData)->processEvent(uMsg, wParam, lParam);
  }

  // Only run the normal WndProc handlers for unhandled events
  if (handled)
    return 0;
  else
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

bool Win32TouchHandler::processEvent(UINT Msg, WPARAM /*wParam*/,
                                     LPARAM lParam)
{
  GESTUREINFO gi;

  DWORD panWant = GC_PAN_WITH_SINGLE_FINGER_VERTICALLY   |
                  GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY |
                  GC_PAN;
  DWORD panBlock = GC_PAN_WITH_INERTIA | GC_PAN_WITH_GUTTER;

  GESTURECONFIG gc[] = {{GID_ZOOM, GC_ZOOM, 0},
                        {GID_PAN, panWant, panBlock},
                        {GID_TWOFINGERTAP, GC_TWOFINGERTAP, 0}};

  switch(Msg) {
  case WM_GESTURENOTIFY:
    if (gesturesConfigured)
        return false;

    if (!SetGestureConfig(hWnd, 0, 3, gc, sizeof(GESTURECONFIG))) {
      vlog.error(_("Failed to set gesture configuration (error 0x%x)"),
                 (int)GetLastError());
    }
    gesturesConfigured = true;
    // Windows expects all handler functions to always
    // pass this message on, and not consume it
    return false;
  case WM_GESTURE:
    ZeroMemory(&gi, sizeof(GESTUREINFO));
    gi.cbSize = sizeof(GESTUREINFO);

    if (!GetGestureInfo((HGESTUREINFO)lParam, &gi)) {
      vlog.error(_("Failed to get gesture information (error 0x%x)"),
                 (int)GetLastError());
      return true;
    }

    handleWin32GestureEvent(gi);

    CloseGestureInfoHandle((HGESTUREINFO)lParam);
    return true;
  }

  return false;
}

void Win32TouchHandler::handleWin32GestureEvent(GESTUREINFO gi)
{
  GestureEvent gev;
  POINT pos;

  if (gi.dwID == GID_BEGIN) {
    return;
  } else if (gi.dwID == GID_END) {
    gestureActive = false;
    ignoringGesture = false;
    return;
  }

  // The GID_BEGIN msg means that no fingers were previously touching,
  // and a completely new set of gestures is beginning.
  // The GF_BEGIN flag means a new type of gesture was detected. This
  // flag can be set on a msg when changing between gestures within
  // one set of touches.
  //
  // We don't support dynamically changing between gestures
  // without lifting the finger(s).
  if ((gi.dwFlags & GF_BEGIN) && gestureActive)
    ignoringGesture = true;
  if (ignoringGesture)
    return;

  if (gi.dwFlags & GF_BEGIN) {
    gev.type = GestureBegin;
  } else if (gi.dwFlags & GF_END) {
    gev.type = GestureEnd;
  } else {
    gev.type = GestureUpdate;
  }

  // Convert to relative coordinates
  pos.x = gi.ptsLocation.x;
  pos.y = gi.ptsLocation.y;
  ScreenToClient(gi.hwndTarget, &pos);
  gev.eventX = pos.x;
  gev.eventY = pos.y;

  switch(gi.dwID) {

  case GID_ZOOM:
    gev.gesture = GesturePinch;
    if (gi.dwFlags & GF_BEGIN) {
      gestureStart.x = pos.x;
      gestureStart.y = pos.y;
    } else {
      gev.eventX = gestureStart.x;
      gev.eventY = gestureStart.y;
    }
    gev.magnitudeX = gi.ullArguments;
    gev.magnitudeY = 0;
    break;

  case GID_PAN:
    if (isSinglePan(gi)) {
      if (gi.dwFlags & GF_BEGIN) {
        gestureStart.x = pos.x;
        gestureStart.y = pos.y;
        startedSinglePan = false;

      }

      // FIXME: Add support for sending a OneFingerTap gesture here.
      //        When the movement was very small and we get a GF_END
      //        within a short time we should consider it a tap.

      if (!startedSinglePan &&
          ((unsigned)abs(gestureStart.x - pos.x) < SINGLE_PAN_THRESHOLD) &&
          ((unsigned)abs(gestureStart.y - pos.y) < SINGLE_PAN_THRESHOLD))
         return;

      // Here we know we got a single pan!

      // Change the first GestureUpdate to GestureBegin
      // after we passed the threshold
      if (!startedSinglePan) {
        startedSinglePan = true;
        gev.type = GestureBegin;
        gev.eventX = gestureStart.x;
        gev.eventY = gestureStart.y;
      }

      gev.gesture = GestureDrag;

    } else {
      if (gi.dwFlags & GF_BEGIN) {
        gestureStart.x = pos.x;
        gestureStart.y = pos.y;
        gev.magnitudeX = 0;
        gev.magnitudeY = 0;
      } else {
        gev.eventX = gestureStart.x;
        gev.eventY = gestureStart.y;
        gev.magnitudeX = pos.x - gestureStart.x;
        gev.magnitudeY = pos.y - gestureStart.y;
      }

      gev.gesture = GestureTwoDrag;
    }
    break;

  case GID_TWOFINGERTAP:
    gev.gesture = GestureTwoTap;
    break;

  }

  gestureActive = true;

  gestureCallback->handleGestureEvent(gev);

  // Since we have a threshold for GestureDrag we need to generate
  // a second event right away with the current position
  if ((gev.type == GestureBegin) && (gev.gesture == GestureDrag)) {
    gev.type = GestureUpdate;
    gev.eventX = pos.x;
    gev.eventY = pos.y;
    gestureCallback->handleGestureEvent(gev);
  }
}

bool Win32TouchHandler::isSinglePan(GESTUREINFO gi)
{
  // To differentiate between a single and a double pan we can look
  // at ullArguments. This shows the distance between the touch points,
  // but in the case of single pan, it seems to show the monitor's
  // origin value (this is not documented by microsoft). This origin
  // value seems to be relative to the screen's position in a multi
  // monitor setup. For example if the touch monitor is secondary and
  // positioned to the left of the primary, the origin is negative.
  //
  // To use this we need to get the monitor's origin value and check
  // if it is the same as ullArguments. If they match, we have a
  // single pan.

  POINT coordinates;
  HMONITOR monitorHandler;
  MONITORINFO mi;
  LONG lowestX;

  // Find the monitor with the touch event
  coordinates.x = gi.ptsLocation.x;
  coordinates.y = gi.ptsLocation.y;
  monitorHandler = MonitorFromPoint(coordinates,
                                    MONITOR_DEFAULTTOPRIMARY);

  // Find the monitor's origin
  ZeroMemory(&mi, sizeof(MONITORINFO));
  mi.cbSize = sizeof(MONITORINFO);
  GetMonitorInfo(monitorHandler, &mi);
  lowestX = mi.rcMonitor.left;

  return lowestX == (LONG)gi.ullArguments;
}
