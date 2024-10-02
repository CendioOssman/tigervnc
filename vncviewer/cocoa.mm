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

//-----------------------------------------------------------------------
@interface NSVNCView : NSView <CALayerDelegate>

@property int width;
@property int height;
@property(nonatomic, assign) unsigned char *framebuffer;
@property(nonatomic, retain) NSImage *image;
@property(nonatomic, assign) NSCursor *cursor;
@property(atomic) int nframes;


- (void)setFrameBuffer:(CGImageRef)iref width:(int)w height:(int)h;
- (void)drawRect:(NSRect)aRect;
- (void)setRemoteCursor:(NSCursor *)newCursor;

@end


@implementation NSVNCView

- (id)initWithFrame:(NSRect)frameRect
{
  if (self = [super initWithFrame:frameRect]) {
    // Indicate that this view can draw on a background thread.
    [self setCanDrawConcurrently:NO];
    NSTrackingArea *trackingArea = [[[NSTrackingArea alloc] initWithRect:frameRect options: NSTrackingActiveInActiveApp | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved owner:self userInfo:nil] autorelease];
    [self addTrackingArea:trackingArea];
    _nframes = 0;
    self.layer.delegate = self;
    self.layer.contentsFormat = kCAContentsFormatRGBA8Uint;
  }
  return self;
}

- (void)dealloc
{
  [_image release];
  [super dealloc];
}

- (void)mouseEntered:(NSEvent*)event
{
  [[NSApplication sharedApplication] activateIgnoringOtherApps:true];
}

- (void)mouseExited:(NSEvent*)event
{
  [[NSApplication sharedApplication] activateIgnoringOtherApps:true];
}

- (void)setRemoteCursor:(NSCursor *)newCursor
{
  if (newCursor) {
    _cursor = [newCursor retain];
    // Force the window to rebuild its cursor rects. This will cause our new cursor to be set.
    [[self window] performSelectorOnMainThread:@selector(resetCursorRects) withObject:nil waitUntilDone:NO];
  }
}

- (void)resetCursorRects
{
  [self addCursorRect:[self visibleRect] cursor: _cursor];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
  return NO;
}

- (BOOL)acceptsFirstResponder
{
  return YES;
}

- (BOOL)isOpaque
{
  return YES;
}

- (BOOL)wantsDefaultClipping
{
  return NO;
}

- (BOOL)wantsLayer
{
  return YES;
}

- (void)setFrameBuffer:(CGImageRef)iref width:(int)w height:(int)h
{
  _width = w;
  _height = h;
  NSRect f = [self frame];
  f.size.width = w;
  f.size.height = h;
  [self setFrame:f];

  _image = [[NSImage alloc] initWithCGImage:iref size:NSMakeSize(_width, _height)];
  [_image setCacheMode:NSImageCacheNever];
  self.layer.contents = _image;
  self.layer.contentsFormat = kCAContentsFormatRGBA8Uint;
}

- (void)drawRect:(NSRect)destRect
{
  NSRect r = NSRect(destRect);
  r.origin.y = _height - r.origin.y - r.size.height;
  [_image drawAtPoint:r.origin fromRect:r operation:NSCompositingOperationCopy fraction:1.0];
  [super setNeedsDisplayInRect:r];
}

- (void)setNeedsDisplayInRect:(NSRect)rect
{
  NSRect r = NSRect(rect);
  r.origin.y = _height - r.origin.y - r.size.height;
  [super setNeedsDisplayInRect:r];
}

- (void)layerWillDraw:(CALayer*)later
{
}

@end

//-----------------------------------------------------------------------

NSView *cocoa_create_view(QWidget *parent, CGImageRef iref)
{
  int w = CGImageGetWidth(iref);
  int h = CGImageGetHeight(iref);

  NSView *parentView = (__bridge NSView*)reinterpret_cast<void *>(parent->winId());
  NSVNCView *view = [[NSVNCView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
  [view setFrameBuffer:iref width:w height:h];
  [parentView addSubview:view];

  [view setCanDrawConcurrently:true];
  [[view window] setAllowsConcurrentViewDrawing:true];
  [[view window] setAcceptsMouseMovedEvents:false]; // Mouse move events are handled by VNCMacView.

  [view removeConstraints:[view constraints]];
  [view setTranslatesAutoresizingMaskIntoConstraints:NO]; // disable auto-layout.
  return view;
}

NSView *cocoa_get_view(QWidget *widget)
{
  NSView *view = (__bridge NSView*)reinterpret_cast<void *>(widget->winId());
  return view;
}

void cocoa_draw(NSView *view, int x, int y, int w, int h)
{
  NSRect r = NSMakeRect(x, y, w, h);
  [view drawRect:r];
}

void cocoa_invalidate_region(NSView *view, int x, int y, int w, int h)
{
  NSRect r = NSMakeRect(x, y, w, h);
  [view setNeedsDisplayInRect:r];
}

void cocoa_beep()
{
  NSBeep();
}

void cocoa_resize(NSView *view, CGImageRef iref)
{
  int width = CGImageGetWidth(iref);
  int height = CGImageGetHeight(iref);
  [(NSVNCView*)view setFrameBuffer:iref width:width height:height];
  NSSize size;
  size.width = width;
  size.height = height;
  [view setFrameSize:size];
  [view setBounds:NSMakeRect(0, 0, width, height)];
  [view setNeedsDisplay:YES];
}

NSCursor *cocoa_set_cursor(NSView *view, const QCursor *cursor)
{
  int hotx = cursor->hotSpot().x();
  int hoty = cursor->hotSpot().y();
  QImage image = cursor->pixmap().toImage();
  int nplanes = (image.depth() + 7) / 8;
  int iwidth = image.width();
  int iheight = image.height();
  bool alpha = image.hasAlphaChannel();
  int bpl = image.bytesPerLine();
  // OS X >= 10.6 can create a NSImage from a CGImage, but we need to
  // support older versions, hence this pesky handling.
  NSBitmapImageRep *bitmap = [[[NSBitmapImageRep alloc]
                              initWithBitmapDataPlanes: nullptr
                              pixelsWide: iwidth
                              pixelsHigh: iheight
                              bitsPerSample: 8
                              samplesPerPixel: nplanes
                              hasAlpha: alpha
                              isPlanar: NO
                              colorSpaceName: nplanes <= 2 ? NSDeviceWhiteColorSpace : NSDeviceRGBColorSpace
                              bytesPerRow: bpl
                              bitsPerPixel: nplanes * 8] autorelease];

  quint32 *ptr = (quint32*)[bitmap bitmapData];
  if (ptr) {
    for (int y = 0; y < image.height(); y++) {
      for (int x = 0; x < image.width(); x++) {
        *ptr++ = image.pixel(x, y);
      }
    }
  }

  NSImage *nsimage = [[[NSImage alloc] initWithSize:NSMakeSize(image.width(), image.height())] autorelease];

  [nsimage addRepresentation:bitmap];

  NSCursor *nscursor = [[[NSCursor alloc] initWithImage:nsimage hotSpot:NSMakePoint(hotx, hoty)] autorelease];
  [(NSVNCView*)view setRemoteCursor:nscursor];

  return nscursor;
}

CGImageRef cocoa_create_bitmap(int width, int height, unsigned char *data)
{
  int bytesPerPixel = 4;   // == R G B PaddingByte
  size_t bytesPerRow = width * bytesPerPixel;
  size_t bufferLength = width * height * 4;
  CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, data, bufferLength, nullptr);
  size_t bitsPerComponent = 8;
  size_t bitsPerPixel = bytesPerPixel * 8;
  CGColorSpaceRef colorSpaceRef = CGDisplayCopyColorSpace(CGMainDisplayID());
  // Note: macOS's default byte order is 'BGRA', but the frameuffer's byte order must be 'RGB+padding_byte' (by code maintainer's request).
  // To align these mismatches, make macOS read a pixel data in reverse order. (Instead of 'kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast')
  CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst;
  CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;

  CGImageRef iref = CGImageCreate(width, 
                                  height, 
                                  bitsPerComponent, 
                                  bitsPerPixel, 
                                  bytesPerRow, 
                                  colorSpaceRef, 
                                  bitmapInfo, 
                                  provider,   // data provider
                                  nullptr,       // decode
                                  YES,        // should interpolate
                                  renderingIntent);
  CGColorSpaceRelease(colorSpaceRef);
  CGDataProviderRelease(provider);
  return iref;
}

int cocoa_capture_displays(QList<int> screens)
{
  CGDirectDisplayID displays[16];
  CGDisplayCount count;
  if (CGGetActiveDisplayList(16, displays, &count) != kCGErrorSuccess) {
    return 0;
  }

  if (screens.size() == (int)count) {
    CGCaptureAllDisplays();
  }
  else {
    for (int dix = 0; dix < (int)count; dix++) {
      if (screens.contains(dix)) {
        if (CGDisplayCapture(displays[dix]) != kCGErrorSuccess) {
          return 0;
        }
      } else {
        // A display might have been captured with the previous
        // monitor selection. In that case we don't want to keep
        // it when its no longer inside the window_rect.
        CGDisplayRelease(displays[dix]);
      }
    }
  }

  captured = true;
  
  return 0;
}

void cocoa_release_displays()
{
  CGReleaseAllDisplays();
  captured = false;
}

void cocoa_update_window_level(QWidget *widget, bool enabled, bool shielding)
{
  NSView* view = cocoa_get_view(widget);
  NSWindow *window = [view window];
  if (enabled) {
    if (shielding) {
      [window setLevel:CGShieldingWindowLevel()];
    } else {
      [window setLevel:NSStatusWindowLevel];
    }
    // We're not getting put in front of the shielding window in many
    // cases on macOS 13, despite setLevel: being documented as also
    // pushing the window to the front. So let's explicitly move it.
    [window orderFront:window];
  } else {
    [window setLevel:NSNormalWindowLevel];
  }
}

bool cocoa_is_mouse_entered(const void *event)
{
  NSEvent *nsevent = (NSEvent*)event;
  NSEventType type = [nsevent type];
  bool det = type == NSEventTypeMouseEntered;
  return det;
}

bool cocoa_is_mouse_exited(const void *event)
{
  NSEvent *nsevent = (NSEvent*)event;
  NSEventType type = [nsevent type];
  bool det = type == NSEventTypeMouseExited;
  return det;
}

bool cocoa_is_mouse_moved(const void *event)
{
  NSEvent *nsevent = (NSEvent*)event;
  NSEventType type = [nsevent type];
  bool det = type == NSEventTypeMouseMoved;
  return det;
}

void cocoa_get_mouse_properties(const void *event, int *x, int *y, int *buttonMask)
{
  NSEvent *nsevent = (NSEvent*)event;
  NSPoint p = [nsevent locationInWindow];
  *x = p.x;
  *y = p.y;
  *buttonMask = [nsevent buttonMask];
}

bool cocoa_displays_have_separate_spaces()
{
  return [NSScreen screensHaveSeparateSpaces];
}

void cocoa_set_overlay_property(WId winid)
{
  NSView *view = reinterpret_cast<NSView*>(winid);
  NSWindow *window = [view window];
  [window setLevel:CGShieldingWindowLevel()+1];
  [window setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary];
}

void cocoa_dim(NSView *view, bool enabled)
{
  CGFloat alpha = enabled ? 0.4 : 1.0;
  [view setAlphaValue:(alpha)];
}

int cocoa_scrollbar_size()
{
  return static_cast<int>([NSScroller
    scrollerWidthForControlSize:static_cast<NSControlSize>(0)
    scrollerStyle:NSScrollerStyleLegacy]);
}

void cocoa_prevent_native_fullscreen(QWidget* w)
{
  NSView* view = cocoa_get_view(w);
  NSWindow* window = [view window];
  [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenNone];
}

void cocoa_fix_warping()
{
    // By default we get a slight delay when we warp the pointer, something
    // we don't want or we'll get jerky movement
    CGEventSourceRef event = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
    CGEventSourceSetLocalEventsSuppressionInterval(event, 0);
    CFRelease(event);
}

void cocoa_set_cursor_pos(int x, int y)
{
    CGPoint new_pos;
    new_pos.x = x;
    new_pos.y = y;
    CGWarpMouseCursorPosition(new_pos);
}
