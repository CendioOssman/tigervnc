#include "vncx11view.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QApplication>
#include <QBitmap>
#include <QGestureEvent>
#include <QGestureRecognizer>
#include <QImage>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif

#include "KeyboardX11.h"
#include "rfb/LogWriter.h"
#include "i18n.h"

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2.h>
#include <X11/XKBlib.h>

static rfb::LogWriter vlog("QVNCX11View");

QVNCX11View::QVNCX11View(QVNCConnection* cc_, QWidget* parent, Qt::WindowFlags f)
  : Viewport(cc_, parent, f)
{
  keyboardHandler = new KeyboardX11(this);
  initKeyboardHandler();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  display = QX11Info::display();
#else
  display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
}

void QVNCX11View::grabPointer()
{
  // We also need to grab the pointer as some WMs like to grab buttons
  // combined with modifies (e.g. Alt+Button0 in metacity).

  // Having a button pressed prevents us from grabbing, we make
  // a new attempt in fltkHandle()

  XIEventMask *curmasks;
  int num_masks;

  int ret, ndevices;

  XIDeviceInfo *devices, *device;
  bool gotGrab;

         // We grab for the same events as the window is currently interested in
  curmasks = XIGetSelectedEvents(display, winId(), &num_masks);
  if (curmasks == nullptr) {
    if (num_masks == -1)
      vlog.error(_("Unable to get X Input 2 event mask for window 0x%08llx"), winId());
    else
      vlog.error(_("Window 0x%08llx has no X Input 2 event mask"), winId());

    return;
  }

         // Our windows should only have a single mask, which allows us to
         // simplify all the code handling the masks
  if (num_masks > 1) {
    vlog.error(_("Window 0x%08llx has more than one X Input 2 event mask"), winId());
    return;
  }

  devices = XIQueryDevice(display, XIAllMasterDevices, &ndevices);

         // Iterate through available devices to find those which
         // provide pointer input, and attempt to grab all such devices.
  gotGrab = false;
  for (int i = 0; i < ndevices; i++) {
    device = &devices[i];

    if (device->use != XIMasterPointer)
      continue;

    curmasks[0].deviceid = device->deviceid;

    ret = XIGrabDevice(display,
                       device->deviceid,
                       winId(),
                       CurrentTime,
                       None,
                       XIGrabModeAsync,
                       XIGrabModeAsync,
                       True,
                       &(curmasks[0]));

    if (ret) {
      if (ret == XIAlreadyGrabbed) {
        continue;
      } else {
        vlog.error(_("Failure grabbing device %i"), device->deviceid);
        continue;
      }
    }

    gotGrab = true;
  }

  XIFreeDeviceInfo(devices);

         // Did we not even grab a single device?
  if (!gotGrab)
    return;

  Viewport::grabPointer();
}

void QVNCX11View::ungrabPointer()
{
  int ndevices;
  XIDeviceInfo *devices, *device;

  devices = XIQueryDevice(display, XIAllMasterDevices, &ndevices);

  // Release all devices, hoping they are the same as when we
  // grabbed things
  for (int i = 0; i < ndevices; i++) {
    device = &devices[i];

    if (device->use != XIMasterPointer)
      continue;

    XIUngrabDevice(display, device->deviceid, CurrentTime);
  }

  XIFreeDeviceInfo(devices);

  Viewport::ungrabPointer();
}

void QVNCX11View::bell()
{
  XBell(display, 0 /* volume */);
}
