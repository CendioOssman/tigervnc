#ifndef VNCMACVIEW_H
#define VNCMACVIEW_H

#include <QAbstractNativeEventFilter>
#include "abstractvncview.h"

class QWindow;
class QScreen;
class QLabel;
class NSView;
class NSCursor;

class QVNCMacView : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCMacView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCMacView();
  double effectiveDevicePixelRatio(QScreen *screen = nullptr) const override { Q_UNUSED(screen) return 1.0; }
  void disableIM() override;
  void enableIM() override;
  void handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;
  void handleKeyRelease(int keyCode) override;

public slots:
  void handleClipboardData(const char* data) override;
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void bell() override;

protected:
  bool event(QEvent *e) override;
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void handleMouseButtonEvent(QMouseEvent*);
  void handleMouseWheelEvent(QWheelEvent*);
  void installNativeEventHandler();
  void fullscreenOnCurrentDisplay() override;
  void fullscreenOnSelectedDisplay(QScreen *screen, int vx, int vy, int vwidth, int vheight) override;

signals:
  void message(const QString &msg, int timeout);

private:
  NSCursor *cursor_;
};

#endif // VNCMACVIEW_H
