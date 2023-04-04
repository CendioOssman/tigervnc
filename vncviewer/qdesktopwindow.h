#ifndef QDESKTOPWINDOW_H
#define QDESKTOPWINDOW_H

#include <QMainWindow>

class QAbstractVNCView;

class QDesktopWindow : public QMainWindow
{
  Q_OBJECT
public:
  QDesktopWindow(QWidget *parent = nullptr);
  ~QDesktopWindow();
  QAbstractVNCView *view() const { return m_view; }

private:
  QAbstractVNCView *m_view;
};

#endif // QDESKTOPWINDOW_H
