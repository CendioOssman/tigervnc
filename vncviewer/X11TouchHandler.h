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

#ifndef __X11TOUCHHANDLER_H__
#define __X11TOUCHHANDLER_H__

#include <X11/Xlib.h>
#include "BaseTouchHandler.h"

class XIDeviceEvent;
class GestureHandler;

class X11TouchHandler: public BaseTouchHandler {
  public:
    X11TouchHandler(GestureHandler *gh);

  protected:
    void preparePointerEvent(XEvent* dst, const GestureEvent src);
    virtual void fakeMotionEvent(const GestureEvent origEvent);
    virtual void fakeButtonEvent(bool press, int button,
                                 const GestureEvent origEvent);
    virtual void fakeKeyEvent(bool press, int keycode,
                              const GestureEvent origEvent);

  private:
    void pushFakeEvent(XEvent* event);

  private:
    int fakeStateMask;
    GestureHandler *gestureHandler;
};

#endif
