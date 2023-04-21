#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include <X11/Xlib.h>
#include "abstractvncview.h"

class QVNCX11View : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCX11View(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCX11View();
  qulonglong nativeWindowHandle() const override;
  Display *display() const;

public slots:
  void setQCursor(const QCursor &cursor) override;
  void grabPointer() override;
  void ungrabPointer() override;
  void bell() override;
  void updateWindow() override;
  void handleKeyPress(int keyCode, quint32 keySym) override;
  void handleKeyRelease(int keyCode) override;

protected:
  bool event(QEvent *e) override;
  bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

  void handleMouseButtonEvent(QMouseEvent*);
  void handleMouseWheelEvent(QWheelEvent*);

signals:
  void message(const QString &msg, int timeout);

public slots:
  void addInvalidRegion(int x0, int y0, int x1, int y1);
  void draw();

private:
  Window m_window;
  rfb::Region *m_region;
  Pixmap toPixmap(QBitmap &bitmap);
};

#endif // VNCX11VIEW_H
