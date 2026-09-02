/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include "Toast.h"

Toast::Toast(QWidget* parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_TranslucentBackground);

  hideTimer = new QTimer(this);
  hideTimer->setInterval(4000);
  hideTimer->setSingleShot(true);
  connect(hideTimer, &QTimer::timeout, this, &Toast::hide);
}

Toast::~Toast()
{
}

void Toast::setText(const char* textbuf)
{
  text = textbuf;

  hideTimer->start();
  show();
  raise();
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
  r = fm.boundingRect(text.c_str()).adjusted(-16, -16, 16, 16);

  r.moveLeft((width() - r.width()) / 2);
  r.moveTop(50);

  painter.setPen(Qt::NoPen);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor("#cc404040"));
  painter.drawRoundedRect(r, 15, 15, Qt::AbsoluteSize);

  p.setColor("#ffffffff");
  painter.setPen(p);
  painter.drawText(r, text.c_str(), QTextOption(Qt::AlignCenter));
}
