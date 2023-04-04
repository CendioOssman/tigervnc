#include "vncwinview.h"
#include "qdesktopwindow.h"

QDesktopWindow::QDesktopWindow(QWidget *parent)
  : QMainWindow(parent)
  , m_view(nullptr)
{
#if defined(WIN32)
  m_view = new QVNCWinView(this);
#endif
  setCentralWidget(m_view);
}

QDesktopWindow::~QDesktopWindow()
{
  delete m_view;
}
