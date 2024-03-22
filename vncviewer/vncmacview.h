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

public slots:
  void handleClipboardData(const char* data) override;
  void bell() override;

protected:
  bool event(QEvent *e) override;
  void resizeEvent(QResizeEvent*) override;
  void fullscreenOnCurrentDisplay() override;
  void fullscreenOnSelectedDisplay(QScreen *screen, int vx, int vy, int vwidth, int vheight) override;
};

#endif // VNCMACVIEW_H
