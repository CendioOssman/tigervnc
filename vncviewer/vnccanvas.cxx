#include <QQuickItem>
#include <QSGGeometryNode>
#include <QPainter>
#include <QColor>
#include <QQuickWindow>
//#include "PlatformPixelBuffer.h"
#include "appmanager.h"
#include "vncconnection.h"
#include "vnccanvas.h"

#ifdef Q_OS_WIN
#define OEMRESOURCE
#include <windows.h>
#include <QtWinExtras/QtWin>
#endif

VncCanvas::VncCanvas(QQuickItem *parent)
  : QQuickPaintedItem(parent)
  , m_geometry(QSGGeometry::defaultAttributes_Point2D(), 3)
{
  setFillColor(QColor(0, 128, 0, 255));
  setFlag(ItemHasContents);
  m_material.setColor(Qt::red);

  // Move to QVNCConnection::initDone()
//  frameBuffer = new PlatformPixelBuffer(width(), height());
//  assert(frameBuffer);
//  QVNCConnection *cc = AppManager::instance()->connection();
//  cc->setFramebuffer(frameBuffer);
}

VncCanvas::~VncCanvas()
{
}

//QSGNode *VncCanvas::updatePaintNode(QSGNode *anode, UpdatePaintNodeData *)
//{
//  QSGGeometryNode *node = dynamic_cast<QSGGeometryNode*>(anode);
//  if (!node) {
//    node = new QSGGeometryNode;
//  }
//  QSGGeometry::Point2D *v = m_geometry.vertexDataAsPoint2D();
//  const QRectF rect = boundingRect();
//  v[0].x = rect.left();
//  v[0].y = rect.bottom();
//  v[1].x = rect.left() + rect.width() / 2;
//  v[1].y = rect.top();
//  v[2].x = rect.right();
//  v[2].y = rect.bottom();
//  node->setGeometry(&m_geometry);
//  node->setMaterial(&m_material);
//  return node;
//}

void VncCanvas::paint(QPainter *painter)
{
#if 0
  PAINTSTRUCT ps;
  painter->beginNativePainting();

  WId wid = window()->winId();

  HWND hWnd = reinterpret_cast<HWND>(wid);
  //HDC hdc = GetDC(hWnd);
  HDC hdc = BeginPaint(hWnd, &ps);

  //HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
  //HINSTANCE hInst = GetModuleHandle(nullptr);
  //HBITMAP hBitmap = LoadBitmap(hInst, "SAMPLEBMP");
  //HBITMAP hBitmap = LoadBitmap(0, MAKEINTRESOURCE(OBM_UPARROW));
  HBITMAP hBitmap = (HBITMAP)LoadImage(0, "C:\\Temp\\sample.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
  if (!hBitmap) {
    DWORD e = GetLastError();
    qDebug() << "error=" << e;
  }
  BITMAP bmp;
  GetObject(hBitmap, sizeof(BITMAP), &bmp);
  int bwidth = (int)bmp.bmWidth;
  int bheight = (int)bmp.bmHeight;
  qDebug() << "bmp width=" << bwidth << ", height=" << bheight;
  HDC hmdc = CreateCompatibleDC(hdc);
  SelectObject(hmdc, hBitmap);
  BitBlt(hdc, 0, 0, bwidth, bheight, hmdc, 0, 0, SRCCOPY);
  //StretchBlt(hdc, 0, bheight, bwidth / 2, bheight / 2, hmdc, 0, 0, bwidth, bheight, SRCCOPY);
  DeleteDC(hmdc);
  DeleteObject(hBitmap);

  EndPaint(hWnd, &ps);

  painter->endNativePainting();
#endif
  WId wid = window()->winId();

  HWND hWnd = reinterpret_cast<HWND>(wid);
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hWnd, &ps);

  HBITMAP hBitmap = (HBITMAP)LoadImage(0, "C:\\Temp\\sample.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
  if (!hBitmap) {
    DWORD e = GetLastError();
    qDebug() << "error=" << e;
  }
  BITMAP bmp;
  GetObject(hBitmap, sizeof(BITMAP), &bmp);
  int bwidth = (int)bmp.bmWidth;
  int bheight = (int)bmp.bmHeight;
  QImage image = QtWin::imageFromHBITMAP(hdc, hBitmap, bwidth, bheight);
  QPoint point(0, 0);
  painter->drawImage(point, image);
}
