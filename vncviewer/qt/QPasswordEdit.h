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

#pragma once

#include <QLineEdit>

class QPasswordEdit : public QLineEdit
{
  Q_OBJECT
public:
  QPasswordEdit(QWidget* parent=nullptr);

protected:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEvent* event) override;
#else
  void enterEvent(QEnterEvent* event) override;
#endif
  void leaveEvent(QEvent* event) override;
  void changeEvent(QEvent* event) override;
  bool event(QEvent* event) override;

protected:
  void toggleVisible();
  void updateVisibility();

  void updateCapsLock();

private:
  QAction* toggle;
  QAction* capsWarning;
};
