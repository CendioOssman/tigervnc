/* Copyright 2022-2026 Pierre Ossman for Cendio AB
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

#include <QFrame>

class QListWidget;
class QStackedWidget;

class QNavigation : public QFrame {
  Q_OBJECT

public:
  QNavigation(QWidget* parent=nullptr);

  int addWidget(const QString& title, QWidget* widget);
  int insertWidget(int index, const QString& title, QWidget* widget);
  void removeWidget(QWidget* widget);

  int currentIndex() const;
  QWidget* currentWidget() const;

  void setCurrentIndex(int index);
  void setCurrentWidget(QWidget* widget);

  int count() const;
  int indexOf(QWidget* widget) const;
  QWidget* widget(int index) const;

protected:
  QListWidget* listWidget;
  QStackedWidget* tabWidget;
};
