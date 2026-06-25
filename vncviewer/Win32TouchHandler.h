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

#ifndef __WIN32TOUCHHANDLER_H__
#define __WIN32TOUCHHANDLER_H__

#include <windows.h>

#include "BaseTouchHandler.h"

class GestureCallback;

class Win32TouchHandler: public BaseTouchHandler {
  public:
    Win32TouchHandler(HWND hWnd, GestureCallback* gestureCallback);

    bool handleEvent(const void* event) override;

  protected:
    static LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg,
                                       WPARAM wParam, LPARAM lParam,
                                       UINT_PTR uIdSubclass,
                                       DWORD_PTR dwRefData);
    bool processEvent(UINT Msg, WPARAM wParam, LPARAM lParam);

  private:
    void handleWin32GestureEvent(GESTUREINFO gi);
    bool isSinglePan(GESTUREINFO gi);

  private:
    HWND hWnd;
    GestureCallback* gestureCallback;

    bool gesturesConfigured;
    bool startedSinglePan;
    POINT gestureStart;

    bool gestureActive;
    bool ignoringGesture;
};

#endif // __WIN32TOUCHHANDLER_H__
