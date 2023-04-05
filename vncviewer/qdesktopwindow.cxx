#include <QLayout>
#include "vncwinview.h"
#include "qdesktopwindow.h"

#include <QDebug>

QDesktopWindow::QDesktopWindow(QWidget *parent)
  : QMainWindow(parent)
  , m_view(nullptr)
{
#if defined(WIN32)
  m_view = new QVNCWinView(this);
#endif
  setCentralWidget(m_view);
  qDebug() << "layout=" << layout();
  //setContentsMargins(0, 0, 0, 0);
}

QDesktopWindow::~QDesktopWindow()
{
  delete m_view;
}
