#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include "abstractvncview.h"

//#define X11_LEGACY_TOUCH 1
#if X11_LEGACY_TOUCH // Not necessary in Qt.
class XInputTouchHandler;
#endif

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
  // Moved from touch.cxx
#if X11_LEGACY_TOUCH // Not necessary in Qt.
  XInputTouchHandler *m_touchHandler;
  int m_ximajor;
#endif

  Pixmap toPixmap(QBitmap &bitmap);
  unsigned int getModifierMask(unsigned int keysym);
  // Moved from touch.h
#if X11_LEGACY_TOUCH // Not necessary in Qt.
  void enable_touch();
  void x11_change_touch_ownership(bool enable);
  bool x11_grab_pointer(Window window);
  void x11_ungrab_pointer(Window window);
#endif
};

#endif // VNCX11VIEW_H
