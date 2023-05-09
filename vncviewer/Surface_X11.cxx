/* Copyright 2016 Pierre Ossman for Cendio AB
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

#include <assert.h>
#include <stdlib.h>
#include <QImage>
#include <QDebug>
//#include <X11/Xlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
//#include <X11/extensions/XShm.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif

#include <rdr/Exception.h>
#include "appmanager.h"
#include "abstractvncview.h"
#include "Surface.h"

static bool caughtError;

static int XShmAttachErrorHandler(Display *dpy, XErrorEvent *error)
{
  caughtError = true;
  return 0;
}

static Display *xdisplay()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  Display *display = QX11Info::display();
#else
  Display *display = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  return display;
}

void Surface::alloc()
{
  if (!setupShm()) {
    Display *display = xdisplay();
    xim = XCreateImage(display, CopyFromParent, 32, ZPixmap, 0, 0, width(), height(), 32, 0);
    if (!xim) {
      throw rdr::Exception("XCreateImage");
    }
    xim->data = (char*)malloc(xim->bytes_per_line * xim->height);
    if (!xim->data) {
      throw rdr::Exception("malloc");
    }
  }
}

void Surface::dealloc()
{
  if (shminfo) {
    Display *display = xdisplay();
    XShmDetach(display, shminfo);
    shmdt(shminfo->shmaddr);
    shmctl(shminfo->shmid, IPC_RMID, 0);
    delete shminfo;
    shminfo = nullptr;
  }
  if (xim) {
    XDestroyImage(xim);
    xim = nullptr;
  }
}

bool Surface::setupShm()
{
  const char *display_name = XDisplayName(nullptr);

  /* Don't use MIT-SHM on remote displays */
  if ((*display_name && *display_name != ':') || QString(qgetenv("QT_X11_NO_MITSHM")).toInt() != 0) {
    return false;
  }
  int major, minor;
  Bool pixmaps;
  Display *display = xdisplay();
  if (!XShmQueryVersion(display, &major, &minor, &pixmaps)) {
    return false;
  }
  shminfo = new XShmSegmentInfo;

  xim = XShmCreateImage(display, CopyFromParent, 32, ZPixmap, 0, shminfo, width(), height());
  if (!xim) {
    dealloc();
    return false;
  }
  shminfo->shmid = shmget(IPC_PRIVATE, xim->bytes_per_line * xim->height, IPC_CREAT | 0600);
  if (shminfo->shmid == -1) {
    dealloc();
    return false;
  }
  shminfo->shmaddr = xim->data = (char*)shmat(shminfo->shmid, 0, 0);
  shmctl(shminfo->shmid, IPC_RMID, 0); // to avoid memory leakage
  if (shminfo->shmaddr == (char *)-1) {
    dealloc();
    return false;
  }
  shminfo->readOnly = True;

  // This is the only way we can detect that shared memory won't work
  // (e.g. because we're accessing a remote X11 server)
  caughtError = false;
  XErrorHandler old_handler = XSetErrorHandler(XShmAttachErrorHandler);

  if (!XShmAttach(display, shminfo)) {
    XSetErrorHandler(old_handler);
    dealloc();
    return false;
  }

  XSync(display, False);
  XSetErrorHandler(old_handler);

  if (caughtError) {
    dealloc();
    return false;
  }
  return true;
}
