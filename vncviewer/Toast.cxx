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

#include <time.h>

#include <QPainter>
#include <QTimer>

#include <core/time.h>

#include "Toast.h"

// Time before we show a toast again
const time_t TOAST_REPEAT_TIMEOUT = 600;

Toast::Toast(QWidget* parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_TranslucentBackground);

  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &Toast::updateOverlay);
}

Toast::~Toast()
{
}

void Toast::addRegular(const char* text, ...)
{
  va_list ap;
  char textbuf[1024];

  std::map<std::string, time_t>::iterator iter;

  va_start(ap, text);
  vsnprintf(textbuf, sizeof(textbuf), text, ap);
  textbuf[sizeof(textbuf)-1] = '\0';
  va_end(ap);

  // Purge all old entries
  for (iter = overlayTimes.begin(); iter != overlayTimes.end(); ) {
    if ((time(nullptr) - iter->second) >= TOAST_REPEAT_TIMEOUT)
      overlayTimes.erase(iter++);
    else
      iter++;
  }

  // Recently shown?
  if (overlayTimes.count(textbuf) > 0)
    return;

  overlayTimes[textbuf] = time(nullptr);

  addOverlay(textbuf);
}

void Toast::addPriority(const char* text, ...)
{
  va_list ap;
  char textbuf[1024];

  va_start(ap, text);
  vsnprintf(textbuf, sizeof(textbuf), text, ap);
  textbuf[sizeof(textbuf)-1] = '\0';
  va_end(ap);

  addOverlay(textbuf);
}

void Toast::addOverlay(const char* text)
{
  overlays.push_back({text, 0, {0, 0}});

  if (overlays.size() == 1) {
    updateTimer->start(500);
    show();
    raise();
  }
}

void Toast::updateOverlay()
{
  struct Overlay* overlay;
  unsigned elapsed;

  if (overlays.empty())
    return;

  overlay = &overlays.front();

  if (overlay->start.tv_sec == 0)
    gettimeofday(&overlay->start, nullptr);

  elapsed = core::msSince(&overlay->start);

  if (elapsed < 500) {
    overlay->alpha = (unsigned)255 * elapsed / 500;
    updateTimer->start(1000/60);
  } else if (elapsed < 3500) {
    overlay->alpha = 255;
    updateTimer->start(3000);
  } else if (elapsed < 4000) {
    overlay->alpha = (unsigned)255 * (4000 - elapsed) / 500;
    updateTimer->start(1000/60);
  } else {
    overlays.pop_front();
    if (!overlays.empty())
      updateTimer->start(500);
  }

  repaint();
}

void Toast::paintEvent(QPaintEvent* /*event*/)
{
  Overlay* overlay;

  QPainter painter(this);
  QFont f;
  QPen p;

  QRect r;

  if (overlays.empty())
    return;

  overlay = &overlays.front();

  // FIXME: Should compose things before alpha
  painter.setOpacity(overlay->alpha / 255.0);

  f.setBold(true);
  f.setPixelSize(14);
  painter.setFont(f);

  QFontMetrics fm(f);
  r = fm.boundingRect(overlay->text.c_str()).adjusted(-16, -16, 16, 16);

  r.moveLeft((width() - r.width()) / 2);
  r.moveTop(50);

  painter.setPen(Qt::NoPen);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor("#cc404040"));
  painter.drawRoundedRect(r, 15, 15, Qt::AbsoluteSize);

  p.setColor("#ffffffff");
  painter.setPen(p);
  painter.drawText(r, overlay->text.c_str(), QTextOption(Qt::AlignCenter));
}
