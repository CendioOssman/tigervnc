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
#include <QElapsedTimer>
#include <QDebug>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include <rfb/keysymdef.h>
#include <rfb/XF86keysym.h>
#include <rfb/Rect.h>

#include "keysym2ucs.h"

#define NoSymbol 0

// This wasn't added until 10.12
#if !defined(MAC_OS_X_VERSION_10_12) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
const int kVK_RightCommand = 0x36;
#endif
// And this is still missing
const int kVK_Menu = 0x6E;

static bool captured = false;
static int mac_os_version = 0;

//-----------------------------------------------------------------------
@interface NSVNCView : NSView

@property int width;
@property int height;
@property(nonatomic, assign) unsigned char *framebuffer;
@property(nonatomic, retain) NSBitmapImageRep *bitmap;
@property(nonatomic, retain) NSImage *image;
@property(nonatomic, assign) NSCursor *cursor;
@property(atomic) QElapsedTimer *elapsed;
@property(atomic) int nframes;


- (void)setFrameBuffer:(NSBitmapImageRep*)bitmap width:(int)w height:(int)h;
- (void)drawRect:(NSRect)aRect;
- (void)setRemoteCursor:(NSCursor *)newCursor;

@end


@implementation NSVNCView

- (id)initWithFrame:(NSRect)frameRect
{
  if (self = [super initWithFrame:frameRect]) {
    // Indicate that this view can draw on a background thread.
    [self setCanDrawConcurrently:YES];
//    NSTrackingArea *trackingArea = [[[NSTrackingArea alloc] initWithRect:frameRect options: NSTrackingActiveInActiveApp | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved owner:self userInfo:nil] autorelease];
//    [self addTrackingArea:trackingArea];
    _elapsed = new QElapsedTimer;
    _nframes = 0;
  }
  return self;
}

- (void)dealloc
{
  delete _elapsed;
  [_image release];
  [_bitmap release];
  [super dealloc];
}

- (void)mouseEntered:(NSEvent*)event
{
  qDebug() << "-mouseEntered";
  [[NSApplication sharedApplication] activateIgnoringOtherApps:true];
}

- (void)mouseExited:(NSEvent*)event
{
  qDebug() << "-mouseExited";
  [[NSApplication sharedApplication] activateIgnoringOtherApps:true];
}

- (void)setRemoteCursor:(NSCursor *)newCursor
{
  if (newCursor) {
    if (_cursor) {
      [_cursor release];
    }
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

- (void)setFrameBuffer:(NSBitmapImageRep*)bitmap width:(int)w height:(int)h
{
  _bitmap = bitmap;
  _framebuffer = [bitmap bitmapData];
  _width = w;
  _height = h;
  NSRect f = [self frame];
  f.size.width = w;
  f.size.height = h;
  [self setFrame:f];

  if (_image) {
    [_image release];
  }
  _image = [[NSImage alloc] initWithSize:f.size];

#if 0
  int bpl = w * 4;
  unsigned char *p0 = _framebuffer;
  for (int y = 0; y < h; y++) {
    unsigned char *p = p0 + y * bpl;
    for (int x = 0; x < w; x++) {
      *p++ = 120;
      *p++ = 0;
      *p++ = 0;
      *p++ = 255;
    }
  }
#endif

  [_image addRepresentation:bitmap];
  self.layer.contents = _image;
}

- (void)drawRect:(NSRect)destRect
{
  qDebug() << "drawRect: x=" << destRect.origin.x << ",y=" << destRect.origin.y << ",w=" << destRect.size.width << ",h=" << destRect.size.height;
  [_bitmap bitmapData];
  //[_bitmap drawInRect:destRect];
  //[_image drawInRect:destRect];
  self.layer.contents = NULL;
  self.layer.contents = _image;
}

@end

//-----------------------------------------------------------------------

NSView *cocoa_create_view(QWidget *parent, NSBitmapImageRep *bitmap)
{
  int w = [bitmap pixelsWide];
  int h = [bitmap pixelsHigh];

  NSView *parentView = (__bridge NSView*)reinterpret_cast<void *>(parent->winId());
  NSVNCView *view = [[NSVNCView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
  [view setFrameBuffer:bitmap width:w height:h];
  [parentView addSubview:view];

  [view setCanDrawConcurrently:true];
  [view setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawNever];
  [[view window] setAllowsConcurrentViewDrawing:true];
  [[view window] setAcceptsMouseMovedEvents:false]; // Mouse move events are handled by VNCMacView.

  [view removeConstraints:[view constraints]];
  [view setTranslatesAutoresizingMaskIntoConstraints:NO]; // disable auto-layout.
  return view;
}

void cocoa_draw(NSView *view, int x, int y, int w, int h)
{
  NSRect r = NSMakeRect(x, y, w, h);
  if ([view canDraw]) {
    [view drawRect:r];
  }
  else {
    dispatch_async(dispatch_get_main_queue(), ^{
      [view drawRect:r];
    });
  }
}

void cocoa_invalidate_region(NSView *view, int x, int y, int w, int h)
{
#if 0
  NSRect r = NSMakeRect(x, y, w, h);
//  if ([view canDraw]) {
    [view needsToDrawRect:r];
//  }
//  else {
//    dispatch_async(dispatch_get_main_queue(), ^{
//      [view needsToDrawRect:r];
//    });
//  }
  qDebug() << "cocoa_invalidate_region: x=" << x << ",y=" << y << ",w=" << w << ",h=" << h;
#endif
}

void cocoa_beep()
{
  NSBeep();
}

int cocoa_mac_os_version() {
  if (mac_os_version) {
    return mac_os_version;
  }
  int major = 0, minor = 0, build = 0;
  NSAutoreleasePool *localPool = [[NSAutoreleasePool alloc] init];
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_10
  if ([NSProcessInfo instancesRespondToSelector:@selector(operatingSystemVersion)]) {
    NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
    major = (int)version.majorVersion;
    minor = (int)version.minorVersion;
    build = (int)version.patchVersion;
  }
  else
#endif
  {
    NSDictionary *sv = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
    const char *s = [[sv objectForKey:@"ProductVersion"] UTF8String];
    sscanf(s, "%d.%d.%d", &major, &minor, &build);
  }
  [localPool release];
  mac_os_version = major*10000 + minor*100 + build;
  return mac_os_version;
}

CGContext *cocoa_gc(NSView *view)
{
  NSWindow *window = [view window];
  NSGraphicsContext *nsgc = [NSGraphicsContext graphicsContextWithWindow:window];
  static SEL gc_sel = cocoa_mac_os_version() >= 101000 ? @selector(CGContext) : @selector(graphicsPort);
  CGContextRef gc = (CGContextRef)[nsgc performSelector:gc_sel];
  return gc;
}

void cocoa_resize(NSView *view, int width, int height)
{
  NSRect r;// = [view frame];
  r.origin.x = 0;
  r.origin.y = 0;
  r.size.width = width;
  r.size.height = height;
  [view setFrame:r];
  //[view setNeedsDisplay:YES];
  qDebug() << "cocoa_resize: w=" << width << ", h=" << height;
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
  NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
                              initWithBitmapDataPlanes: NULL
                              pixelsWide: iwidth
                              pixelsHigh: iheight
                              bitsPerSample: 8
                              samplesPerPixel: nplanes
                              hasAlpha: alpha
                              isPlanar: NO
                              colorSpaceName: nplanes <= 2 ? NSDeviceWhiteColorSpace : NSDeviceRGBColorSpace
                              bytesPerRow: bpl
                              bitsPerPixel: nplanes * 8];

  quint32 *ptr = (quint32*)[bitmap bitmapData];
  if (ptr) {
    for (int y = 0; y < image.height(); y++) {
      for (int x = 0; x < image.width(); x++) {
        *ptr++ = image.pixel(x, y);
      }
    }
  }

  NSImage *nsimage = [[NSImage alloc] initWithSize:NSMakeSize(image.width(), image.height())];

  [nsimage addRepresentation:bitmap];

  NSCursor *nscursor = [[[NSCursor alloc] initWithImage:nsimage hotSpot:NSMakePoint(hotx, hoty)] autorelease];
  [(NSVNCView*)view setRemoteCursor:nscursor];

  return nscursor;
}

void cocoa_delete_cursor(NSCursor *cursor)
{
}

NSBitmapImageRep *cocoa_create_bitmap(int width, int height, unsigned char *data)
{
  int samplesPerPixel = 3; // == R G B
  int bytesPerPixel = 4;   // == R G B PaddingByte
  size_t bytesPerRow = width * bytesPerPixel;
  NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
                              initWithBitmapDataPlanes: &data
                              pixelsWide: width
                              pixelsHigh: height
                              bitsPerSample: 8
                              samplesPerPixel: samplesPerPixel
                              hasAlpha: NO
                              isPlanar: NO
                              colorSpaceName: NSDeviceRGBColorSpace //  NSCalibratedRGBColorSpace
                              bytesPerRow: bytesPerRow
                              bitsPerPixel: bytesPerPixel * 8];
  qDebug() << "cocoa_create_bitmap: bitmap=" << bitmap << ", data=" << data << ", [bitmap bitmapData]=" << [bitmap bitmapData];
  return bitmap;
}

unsigned char *cocoa_get_bitmap_data(NSBitmapImageRep *bitmap)
{
  unsigned char *data = [bitmap bitmapData];
  qDebug() << "cocoa_get_bitmap_data: bitmap=" << bitmap << ", data=" << data;
  return data;
}

NSBitmapImageRep *cocoa_get_image(NSImage *image)
{
  for (NSImageRep *rep : [image representations]) {
    if ([rep isKindOfClass:[NSBitmapImageRep class]]) {
      NSBitmapImageRep *imageRep = reinterpret_cast<NSBitmapImageRep*>(rep);
      return imageRep;
    }
  }
  return NULL;
}

int cocoa_get_level(QWidget *parent)
{
  NSWindow *window = (__bridge NSWindow*)reinterpret_cast<void *>(parent->winId());
  return [window level];
}

void cocoa_set_level(QWidget *parent, int level)
{
  NSWindow *window = (__bridge NSWindow*)reinterpret_cast<void *>(parent->winId());
  [window setLevel:level];
}

int cocoa_capture_displays(QWidget *parent)
{
  NSWindow *window = (__bridge NSWindow*)reinterpret_cast<void *>(parent->winId());
  CGDirectDisplayID displays[16];

  NSRect r = [window frame];
  rfb::Rect windows_rect;
  windows_rect.setXYWH(r.origin.x, r.origin.y, r.size.width, r.size.height);

  CGDisplayCount count;
  if (CGGetActiveDisplayList(16, displays, &count) != kCGErrorSuccess) {
    return 1;
  }

  QApplication *app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  if (count != screens.length()) {
    return 1;
  }

  for (int i = 0; i < screens.length(); i++) {
    double dpr = screens[i]->devicePixelRatio();
    QRect vg = screens[i]->geometry();
    int sx = vg.x();
    int sy = vg.y();
    int sw = vg.width() * dpr;
    int sh = vg.height() * dpr;
    
    rfb::Rect screen_rect;
    screen_rect.setXYWH(sx, sy, sw, sh);
    if (screen_rect.enclosed_by(windows_rect)) {
      if (CGDisplayCapture(displays[i]) != kCGErrorSuccess) {
        return 1;
      }
    } else {
      // A display might have been captured with the previous
      // monitor selection. In that case we don't want to keep
      // it when its no longer inside the window_rect.
      CGDisplayRelease(displays[i]);
    }
  }

  captured = true;

  if ([window level] == CGShieldingWindowLevel()) {
    return 0;
  }

  [window setLevel:CGShieldingWindowLevel()];

  // We're not getting put in front of the shielding window in many
  // cases on macOS 13, despite setLevel: being documented as also
  // pushing the window to the front. So let's explicitly move it.
  [window orderFront:window];

  return 0;
}

void cocoa_release_displays(QWidget *parent)
{
  NSWindow *window = (__bridge NSWindow*)reinterpret_cast<void *>(parent->winId());

  if (captured) {
    CGReleaseAllDisplays();
  }
  captured = false;

  // Someone else has already changed the level of this window
  if ([window level] != CGShieldingWindowLevel()) {
    return;
  }

  // FIXME: Store the previous level somewhere so we don't have to hard
  //        code a level here.
  int newlevel = parent->isFullScreen() ? NSStatusWindowLevel : NSNormalWindowLevel;

  // Only change if different as the level change also moves the window
  // to the top of that level.
  if ([window level] != newlevel) {
    [window setLevel:newlevel];
  }
}

CGColorSpaceRef cocoa_win_color_space(NSView *view)
{
  NSWindow *window = [view window];
  NSColorSpace *nscs = [window colorSpace];
  if (nscs == nil) {
    // Offscreen, so return standard SRGB color space
    assert(false);
    return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  }

  CGColorSpaceRef lut = [nscs CGColorSpace];

  // We want a permanent reference, not an autorelease
  CGColorSpaceRetain(lut);

  return lut;
}

bool cocoa_win_is_zoomed(QWidget *parent)
{
  NSWindow *window = (__bridge NSWindow*)reinterpret_cast<void *>(parent->winId());
  return [window isZoomed];
}

void cocoa_win_zoom(QWidget *parent)
{
  NSWindow *window = (__bridge NSWindow*)reinterpret_cast<void *>(parent->winId());
  [window zoom:window];
}

int cocoa_is_keyboard_sync(const void *event)
{
  assert(event);
  const NSEvent *nsevent = (const NSEvent*)event;

  // If we get a NSFlagsChanged event with key code 0 then this isn't
  // an actual keyboard event but rather the system trying to sync up
  // modifier state after it has stolen input for some reason (e.g.
  // Cmd+Tab)

  if ([nsevent type] != NSFlagsChanged) {
    return 0;
  }
  if ([nsevent keyCode] != 0) {
    return 0;
  }
  return 1;
}

int cocoa_is_keyboard_event(const void *event)
{
  NSEvent *nsevent = (NSEvent*)event;
  switch ([nsevent type]) {
  case NSKeyDown:
  case NSKeyUp:
  case NSFlagsChanged:
    if (cocoa_is_keyboard_sync(event)) {
      return 0;
    }
    return 1;
  default:
    return 0;
  }

  return 0;
}

int cocoa_is_key_press(const void *event)
{
  NSEvent *nsevent = (NSEvent*)event;

  if ([nsevent type] == NSKeyDown) {
    return 1;
  }
  if ([nsevent type] == NSFlagsChanged) {
    UInt32 mask;

    // We don't see any event on release of CapsLock
    if ([nsevent keyCode] == kVK_CapsLock) {
      return 1;
    }
    // These are entirely undocumented, but I cannot find any other way
    // of differentiating between left and right keys
    switch ([nsevent keyCode]) {
    case kVK_RightCommand:
      mask = 0x0010;
      break;
    case kVK_Command:
      mask = 0x0008;
      break;
    case kVK_Shift:
      mask = 0x0002;
      break;
    case kVK_CapsLock:
      // We don't see any event on release of CapsLock
      return 1;
    case kVK_Option:
      mask = 0x0020;
      break;
    case kVK_Control:
      mask = 0x0001;
      break;
    case kVK_RightShift:
      mask = 0x0004;
      break;
    case kVK_RightOption:
      mask = 0x0040;
      break;
    case kVK_RightControl:
      mask = 0x2000;
      break;
    default:
      return 0;
    }

    if ([nsevent modifierFlags] & mask) {
      return 1;
    }
    else {
      return 0;
    }
  }

  return 0;
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

int cocoa_event_keycode(const void *event)
{
  NSEvent *nsevent = (NSEvent*)event;
  int keycode = [nsevent keyCode];

  // macOS swaps these two keys for unknown reasons for ISO layouts
  if (KBGetLayoutType(LMGetKbdType()) == kKeyboardISO) {
    if (keycode == kVK_ANSI_Grave) {
      return kVK_ISO_Section;
    }
    if (keycode == kVK_ISO_Section) {
      return kVK_ANSI_Grave;
    }
  }

  return keycode;
}

static NSString *key_translate(UInt16 keyCode, UInt32 modifierFlags)
{
  TISInputSourceRef keyboard = TISCopyCurrentKeyboardLayoutInputSource();
  CFDataRef uchr = (CFDataRef)TISGetInputSourceProperty(keyboard, kTISPropertyUnicodeKeyLayoutData);
  if (uchr == nullptr) {
    return nil;
  }
  const UCKeyboardLayout *layout = (const UCKeyboardLayout*)CFDataGetBytePtr(uchr);
  if (layout == nullptr) {
    return nil;
  }

  UniCharCount actual_len;
  UniChar string[255];
  UInt32 dead_state = 0;
  UniCharCount max_len = sizeof(string) / sizeof(*string);
  modifierFlags = (modifierFlags >> 8) & 0xff;
  OSStatus err = UCKeyTranslate(layout, keyCode, kUCKeyActionDown, modifierFlags,
                       LMGetKbdType(), 0, &dead_state, max_len, &actual_len,
                       string);
  if (err != noErr) {
    return nil;
  }
  // Dead key?
  if (dead_state != 0) {
    // We have no fool proof way of asking what dead key this is.
    // Assume we get a spacing equivalent if we press the
    // same key again, and try to deduce something from that.
    err = UCKeyTranslate(layout, keyCode, kUCKeyActionDown, modifierFlags,
                         LMGetKbdType(), 0, &dead_state, max_len, &actual_len,
                         string);
    if (err != noErr) {
      return nil;
    }
  }

  return [NSString stringWithCharacters:string length:actual_len];
}

static const int kvk_map[][2] = {
  { kVK_Return,         XK_Return },
  { kVK_Tab,            XK_Tab },
  { kVK_Space,          XK_space },
  { kVK_Delete,         XK_BackSpace },
  { kVK_Escape,         XK_Escape },
  { kVK_RightCommand,   XK_Super_R },
  { kVK_Command,        XK_Super_L },
  { kVK_Shift,          XK_Shift_L },
  { kVK_CapsLock,       XK_Caps_Lock },
  { kVK_Option,         XK_Alt_L },
  { kVK_Control,        XK_Control_L },
  { kVK_RightShift,     XK_Shift_R },
  { kVK_RightOption,    XK_Alt_R },
  { kVK_RightControl,   XK_Control_R },
  { kVK_F17,            XK_F17 },
  { kVK_VolumeUp,       XF86XK_AudioRaiseVolume },
  { kVK_VolumeDown,     XF86XK_AudioLowerVolume },
  { kVK_Mute,           XF86XK_AudioMute },
  { kVK_F18,            XK_F18 },
  { kVK_F19,            XK_F19 },
  { kVK_F20,            XK_F20 },
  { kVK_F5,             XK_F5 },
  { kVK_F6,             XK_F6 },
  { kVK_F7,             XK_F7 },
  { kVK_F3,             XK_F3 },
  { kVK_F8,             XK_F8 },
  { kVK_F9,             XK_F9 },
  { kVK_F11,            XK_F11 },
  { kVK_F13,            XK_F13 },
  { kVK_F16,            XK_F16 },
  { kVK_F14,            XK_F14 },
  { kVK_F10,            XK_F10 },
  { kVK_Menu,           XK_Menu },
  { kVK_F12,            XK_F12 },
  { kVK_F15,            XK_F15 },
  // Should we send Insert here?
  { kVK_Help,           XK_Help },
  { kVK_Home,           XK_Home },
  { kVK_PageUp,         XK_Page_Up },
  { kVK_ForwardDelete,  XK_Delete },
  { kVK_F4,             XK_F4 },
  { kVK_End,            XK_End },
  { kVK_F2,             XK_F2 },
  { kVK_PageDown,       XK_Page_Down },
  { kVK_F1,             XK_F1 },
  { kVK_LeftArrow,      XK_Left },
  { kVK_RightArrow,     XK_Right },
  { kVK_DownArrow,      XK_Down },
  { kVK_UpArrow,        XK_Up },

  // The OS X headers claim these keys are not layout independent.
  // Could it be because of the state of the decimal key?
  /* { kVK_ANSI_KeypadDecimal,     XK_KP_Decimal }, */ // see below
  { kVK_ANSI_KeypadMultiply,    XK_KP_Multiply },
  { kVK_ANSI_KeypadPlus,        XK_KP_Add },
  // OS X doesn't have NumLock, so is this really correct?
  { kVK_ANSI_KeypadClear,       XK_Num_Lock },
  { kVK_ANSI_KeypadDivide,      XK_KP_Divide },
  { kVK_ANSI_KeypadEnter,       XK_KP_Enter },
  { kVK_ANSI_KeypadMinus,       XK_KP_Subtract },
  { kVK_ANSI_KeypadEquals,      XK_KP_Equal },
  { kVK_ANSI_Keypad0,           XK_KP_0 },
  { kVK_ANSI_Keypad1,           XK_KP_1 },
  { kVK_ANSI_Keypad2,           XK_KP_2 },
  { kVK_ANSI_Keypad3,           XK_KP_3 },
  { kVK_ANSI_Keypad4,           XK_KP_4 },
  { kVK_ANSI_Keypad5,           XK_KP_5 },
  { kVK_ANSI_Keypad6,           XK_KP_6 },
  { kVK_ANSI_Keypad7,           XK_KP_7 },
  { kVK_ANSI_Keypad8,           XK_KP_8 },
  { kVK_ANSI_Keypad9,           XK_KP_9 },
  // Japanese Keyboard Support
  { kVK_JIS_Eisu,               XK_Eisu_toggle },
  { kVK_JIS_Kana,               XK_Hiragana_Katakana },
};

int cocoa_event_keysym(const void *event)
{
  NSEvent *nsevent = (NSEvent*)event;
  UInt16 key_code = [nsevent keyCode];

  // Start with keys that either don't generate a symbol, or
  // generate the same symbol as some other key.
  for (size_t i = 0; i < sizeof(kvk_map) / sizeof(kvk_map[0]); i++) {
    if (key_code == kvk_map[i][0]) {
      return kvk_map[i][1];
    }
  }

  // OS X always sends the same key code for the decimal key on the
  // num pad, but X11 wants different keysyms depending on if it should
  // be a comma or full stop.
  if (key_code == 0x41) {
    switch ([[nsevent charactersIgnoringModifiers] UTF8String][0]) {
    case ',':
      return XK_KP_Separator;
    case '.':
      return XK_KP_Decimal;
    default:
      return NoSymbol;
    }
  }

  // We want a "normal" symbol out of the event, which basically means
  // we only respect the shift and alt/altgr modifiers. Cocoa can help
  // us if we only wanted shift, but as we also want alt/altgr, we'll
  // have to do some lookup ourselves. This matches our behaviour on
  // other platforms.

  UInt32 modifiers = 0;
  if ([nsevent modifierFlags] & NSAlphaShiftKeyMask) {
    modifiers |= alphaLock;
  }
  if ([nsevent modifierFlags] & NSShiftKeyMask) {
    modifiers |= shiftKey;
  }
  if ([nsevent modifierFlags] & NSAlternateKeyMask) {
    modifiers |= optionKey;
  }

  NSString *chars = key_translate(key_code, modifiers);
  if (chars == nil) {
    return NoSymbol;
  }

  // FIXME: Some dead keys are given as NBSP + combining character
  if ([chars length] != 1) {
    return NoSymbol;
  }
  // Dead key?
  if ([[nsevent characters] length] == 0) {
    return ucs2keysym(ucs2combining([chars characterAtIndex:0]));
  }
  return ucs2keysym([chars characterAtIndex:0]);
}

static int cocoa_open_hid(io_connect_t *ioc)
{
  CFMutableDictionaryRef mdict = IOServiceMatching(kIOHIDSystemClass);
  io_service_t ios = IOServiceGetMatchingService(kIOMasterPortDefault, (CFDictionaryRef) mdict);
  if (!ios) {
    return KERN_FAILURE;
  }
  kern_return_t ret = IOServiceOpen(ios, mach_task_self(), kIOHIDParamConnectType, ioc);
  IOObjectRelease(ios);
  if (ret != KERN_SUCCESS) {
    return ret;
  }
  return KERN_SUCCESS;
}

static int cocoa_set_modifier_lock_state(int modifier, bool on)
{
  io_connect_t ioc;
  kern_return_t ret = cocoa_open_hid(&ioc);
  if (ret != KERN_SUCCESS) {
    return ret;
  }
  ret = IOHIDSetModifierLockState(ioc, modifier, on);
  IOServiceClose(ioc);
  if (ret != KERN_SUCCESS) {
    return ret;
  }
  return KERN_SUCCESS;
}

static int cocoa_get_modifier_lock_state(int modifier, bool *on)
{
  io_connect_t ioc;
  kern_return_t ret = cocoa_open_hid(&ioc);
  if (ret != KERN_SUCCESS) {
    return ret;
  }
  ret = IOHIDGetModifierLockState(ioc, modifier, on);
  IOServiceClose(ioc);
  if (ret != KERN_SUCCESS) {
    return ret;
  }
  return KERN_SUCCESS;
}

int cocoa_set_caps_lock_state(bool on)
{
  return cocoa_set_modifier_lock_state(kIOHIDCapsLockState, on);
}

int cocoa_set_num_lock_state(bool on)
{
  return cocoa_set_modifier_lock_state(kIOHIDNumLockState, on);
}

int cocoa_get_caps_lock_state(bool *on)
{
  return cocoa_get_modifier_lock_state(kIOHIDCapsLockState, on);
}

int cocoa_get_num_lock_state(bool *on)
{
  return cocoa_get_modifier_lock_state(kIOHIDNumLockState, on);
}

void cocoa_get_mouse_properties(const void *event, int *x, int *y, int *buttonMask)
{
  NSEvent *nsevent = (NSEvent*)event;
  NSPoint p = [nsevent locationInWindow];
  *x = p.x;
  *y = p.y;
  *buttonMask = [nsevent buttonMask];
}
