/* Copyright 2025 Samuel Mannehed for Cendio AB
 * Copyright 2026 Pierre Ossman for Cendio AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QAction>
#include <QIcon>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif

#if defined(WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#else
// This is a keyword in C++
#define explicit xcb_explicit
#include <xcb/xkb.h>
#undef explicit
#endif

#include <core/i18n.h>

#include "QPasswordEdit.h"

QPasswordEdit::QPasswordEdit(QWidget *parent)
  : QLineEdit(parent), toggle(nullptr), capsWarning(nullptr)
{
  setEchoMode(QLineEdit::Password);

  connect(this, &QLineEdit::textChanged, this,
          &QPasswordEdit::updateVisibility);

  toggle = addAction(QIcon(QPixmap(":/open_eye.svg")),
                     QLineEdit::TrailingPosition);
  connect(toggle, &QAction::triggered, this,
          &QPasswordEdit::toggleVisible);
  toggle->setVisible(false); // No eye image is shown by default

  capsWarning = addAction(QIcon(QPixmap(":/shift_lock.svg")),
                          QLineEdit::TrailingPosition);
  // FIXME: Should probably use tr() to not be TigerVNC specific
  capsWarning->setToolTip(_("Caps lock is active"));
  capsWarning->setVisible(false);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void QPasswordEdit::enterEvent(QEvent*)
#else
void QPasswordEdit::enterEvent(QEnterEvent*)
#endif
{
  // Show the eye on mouse hover
  toggle->setVisible(true);
}

void QPasswordEdit::leaveEvent(QEvent*)
{
  updateVisibility();
}

void QPasswordEdit::changeEvent(QEvent*)
{
  if (!isEnabled()) {
    setEchoMode(QLineEdit::Password);
    toggle->setIcon(QIcon(QPixmap(":/open_eye.svg")));
  }
}

bool QPasswordEdit::event(QEvent* event)
{
  updateCapsLock();
  return QLineEdit::event(event);
}

void QPasswordEdit::toggleVisible()
{
  if (echoMode() == QLineEdit::Password) {
    setEchoMode(QLineEdit::Normal);
    toggle->setIcon(QIcon(QPixmap(":/closed_eye.svg")));
  } else {
    setEchoMode(QLineEdit::Password);
    toggle->setIcon(QIcon(QPixmap(":/open_eye.svg")));
  }
}

void QPasswordEdit::updateVisibility()
{
  /* Always show the eye if the password is toggled to be visible,
   * or if the input contains text. */
  if ((echoMode() != QLineEdit::Password) || !text().isEmpty())
    toggle->setVisible(true);
  else
    toggle->setVisible(false);
}

void QPasswordEdit::updateCapsLock()
{
  bool capslock_active;

  if (!capsWarning)
    return;

  capslock_active = false;
#if defined(WIN32)
  if (GetKeyState(VK_CAPITAL) & 0x1)
    capslock_active = true;
#elif defined(__APPLE__)
  kern_return_t ret;
  io_connect_t ioc;
  io_service_t ios;
  CFMutableDictionaryRef mdict;
  bool on;

  mdict = IOServiceMatching(kIOHIDSystemClass);
  ios = IOServiceGetMatchingService(0, (CFDictionaryRef) mdict);

  if (!ios)
    return;

  ret = IOServiceOpen(ios, mach_task_self(), kIOHIDParamConnectType, &ioc);
  IOObjectRelease(ios);
  if (ret != KERN_SUCCESS)
    return;

  ret = IOHIDGetModifierLockState(ioc, kIOHIDCapsLockState, &on);
  IOServiceClose(ioc);
  if (ret != KERN_SUCCESS)
    return;

  capslock_active = on;
#else
  xcb_connection_t* conn;
  xcb_xkb_get_state_cookie_t cookie;
  xcb_xkb_get_state_reply_t* reply;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  conn = QX11Info::connection();
#else
  conn = qApp->nativeInterface<QNativeInterface::QX11Application>()->connection();
#endif

  cookie = xcb_xkb_get_state(conn, XCB_XKB_ID_USE_CORE_KBD);
  reply = xcb_xkb_get_state_reply(conn, cookie, NULL);
  if (!reply)
    return;

  if (reply->lockedMods & XCB_MOD_MASK_LOCK)
    capslock_active = true;

  free(reply);
#endif

  capsWarning->setVisible(capslock_active);
}
