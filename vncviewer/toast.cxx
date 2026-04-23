/* Copyright 2024-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <QPainter>
#include <QTimer>

#include <rfb/util.h>

#include "toast.h"

Toast::Toast(QWidget* parent)
  : QWidget{parent}
{
  hide();

  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  // FIXME: This prevents rendering of lower widgets on Qt6 on X11
  //setAttribute(Qt::WA_TranslucentBackground);
  // FIXME: These two are for windows, not widgets. Are they needed?
  setWindowFlag(Qt::WindowTransparentForInput, true);
  setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);

  toastTimer = new QTimer(this);
  toastTimer->setInterval(4000);
  toastTimer->setSingleShot(true);
  connect(toastTimer, &QTimer::timeout, this, &Toast::hideToast);
}

void Toast::showToast(const char* text)
{
  toastText = text;
  toastTimer->start();
  show();
  raise();
}

void Toast::hideToast()
{
  toastTimer->stop();
  hide();
}

void Toast::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  QFont f;
  QPen p;

  QRect r;

  f.setBold(true);
  f.setPixelSize(14);
  painter.setFont(f);

  QFontMetrics fm(f);
  r = fm.boundingRect(toastText.c_str()).adjusted(-16, -16, 16, 16);

  r.moveLeft((width() - r.width()) / 2);
  r.moveTop(50);

  painter.setPen(Qt::NoPen);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor("#cc404040"));
  painter.drawRoundedRect(r, 15, 15, Qt::AbsoluteSize);

  p.setColor("#ffffffff");
  painter.setPen(p);
  painter.drawText(r, toastText.c_str(), QTextOption(Qt::AlignCenter));
}
