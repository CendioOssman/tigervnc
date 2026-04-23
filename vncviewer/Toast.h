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

#ifndef __TOAST_H__
#define __TOAST_H__

#include <string>

#include <QWidget>

class Toast : public QWidget
{
  Q_OBJECT

public:
  Toast(QWidget* parent = nullptr);

  void showToast(const char* text);
  void hideToast();

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  QTimer* toastTimer;
  std::string toastText;
};

#endif
