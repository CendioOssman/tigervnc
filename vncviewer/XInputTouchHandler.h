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

#ifndef __XINPUTTOUCHHANDLER_H__
#define __XINPUTTOUCHHANDLER_H__

#include <xcb/xcb.h>

#include "BaseTouchHandler.h"
#include "GestureHandler.h"

class XInputTouchHandler: public BaseTouchHandler {
  public:
    XInputTouchHandler(xcb_window_t wnd, GestureCallback* gestureCallback);

    bool handleEvent(const char* eventType, void* message) override;

  protected:
    void processEvent(const xcb_ge_generic_event_t* devev);

  private:
    xcb_window_t wnd;

    GestureHandler gestureHandler;
};

#endif
