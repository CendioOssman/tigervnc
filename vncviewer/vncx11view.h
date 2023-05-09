#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include "abstractvncview.h"

class QVNCX11View : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCX11View(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCX11View();
  qulonglong nativeWindowHandle() const override;

public slots:
  void setQCursor(const QCursor &cursor) override;
  void handleClipboardData(const char* data) override;
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void handleKeyPress(int keyCode, quint32 keySym) override;
  void handleKeyRelease(int keyCode) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void grabPointer() override;
  void ungrabPointer() override;
  void bell() override;
  void updateWindow() override;
  void resizePixmap(int width, int height);
  void draw();

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

private:
  Window m_window;
  Display *m_display;
  int m_screen;
  XVisualInfo *m_visualInfo;
  XRenderPictFormat *m_visualFormat;
  Colormap m_colorMap;
  Pixmap m_pixmap;
  Picture m_picture;

  Pixmap toPixmap(QBitmap &bitmap);
  unsigned int getModifierMask(unsigned int keysym);
};

#endif // VNCX11VIEW_H
