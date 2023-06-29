#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include <QBitmap>
#include <X11/extensions/Xrender.h>
#include "abstractvncview.h"

class XInputTouchHandler;
class QVNCGestureRecognizer;

class QVNCX11View : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCX11View(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCX11View();
  qulonglong nativeWindowHandle() const override;
  void disableIM() override;
  void enableIM() override;
  void handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;
  void handleKeyRelease(int keyCode) override;

public slots:
  void setQCursor(const QCursor &cursor) override;
  void handleClipboardData(const char* data) override;
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#else
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;

  void handleMouseButtonEvent(QMouseEvent*);
  void handleMouseWheelEvent(QWheelEvent*);
  void setWindowManager() override;
  void fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight) override;

signals:
  void message(const QString &msg, int timeout);

private:
  Window window_;
  Display *display_;
  int screen_;
  XVisualInfo *visualInfo_;
  XRenderPictFormat *visualFormat_;
  Colormap colorMap_;
  Pixmap pixmap_;
  Picture picture_;

  GestureHandler *gestureHandler_;
  int eventNumber_;
  static QVNCGestureRecognizer *vncGestureRecognizer_;

  Pixmap toPixmap(QBitmap &bitmap);
  unsigned int getModifierMask(unsigned int keysym);
  bool gestureEvent(QGestureEvent *event);
};

#endif // VNCX11VIEW_H
