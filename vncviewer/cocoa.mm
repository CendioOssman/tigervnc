/* Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <QApplication>
#include <QScreen>
#include <QWidget>
#include <QCursor>
#include <QPixmap>
#include <QImage>

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

#include <rfb/Rect.h>

static bool captured = false;

void cocoa_beep()
{
  NSBeep();
}

int cocoa_capture_displays(QWidget* win)
{
  NSView *view;
  NSWindow *nsw;

  QList<QScreen*> screens;

  view = (NSView*)win->winId();
  nsw = [view window];

  screens = qApp->screens();
  for (QScreen* screen : screens) {
    NSScreen *nsscreen;
    CGDirectDisplayID display;

    nsscreen = screen->nativeInterface<QNativeInterface::QCocoaScreen>()->nativeScreen();
    display = [(NSNumber*)[nsscreen deviceDescription][@"NSScreenNumber"] unsignedIntValue];

    if (win->geometry().contains(screen->geometry())) {
      if (CGDisplayCapture(display) != kCGErrorSuccess)
        return 1;

    } else {
      // A display might have been captured with the previous
      // monitor selection. In that case we don't want to keep
      // it when its no longer inside the window_rect.
      CGDisplayRelease(display);
    }
  }

  captured = true;

  if ([nsw level] == CGShieldingWindowLevel())
    return 0;

  [nsw setLevel:CGShieldingWindowLevel()];

  // We're not getting put in front of the shielding window in many
  // cases on macOS 13, despite setLevel: being documented as also
  // pushing the window to the front. So let's explicitly move it.
  [nsw orderFront:nsw];

  return 0;
}

void cocoa_release_displays(QWidget* win)
{
  NSView *view;
  NSWindow *nsw;
  int newlevel;

  if (captured)
    CGReleaseAllDisplays();

  captured = false;

  view = (NSView*)win->winId();
  nsw = [view window];

  // Someone else has already changed the level of this window
  if ([nsw level] != CGShieldingWindowLevel())
    return;

  // FIXME: what's the right level for Qt?
  newlevel = NSNormalWindowLevel;

  // Only change if different as the level change also moves the window
  // to the top of that level.
  if ([nsw level] != newlevel)
    [nsw setLevel:newlevel];
}

bool cocoa_screens_have_separate_spaces()
{
  return [NSScreen screensHaveSeparateSpaces];
}

int cocoa_scrollbar_size()
{
  return static_cast<int>([NSScroller
    scrollerWidthForControlSize:static_cast<NSControlSize>(0)
    scrollerStyle:NSScrollerStyleLegacy]);
}

void cocoa_prevent_native_full_screen(QWidget* win)
{
  NSView* view = (NSView*)win->winId();
  NSWindow* nsw = [view window];
  [nsw setCollectionBehavior:NSWindowCollectionBehaviorFullScreenNone];
}

void cocoa_event_delay(double seconds)
{
  CGEventSourceRef event = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
  CGEventSourceSetLocalEventsSuppressionInterval(event, seconds);
  CFRelease(event);
}

void cocoa_set_cursor_pos(int x, int y)
{
  CGPoint new_pos;
  new_pos.x = x;
  new_pos.y = y;
  CGWarpMouseCursorPosition(new_pos);
}
