#ifndef VNCMACVIEW_H
#define VNCMACVIEW_H

#include "abstractvncview.h"

#include <QAbstractNativeEventFilter>

class QWindow;
class QScreen;
class QLabel;
class NSView;
class NSCursor;

class QVNCMacView : public QAbstractVNCView
{
  Q_OBJECT

public:
  QVNCMacView(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCMacView();

public slots:
  void handleClipboardData(const char* data) override;
  void bell() override;

protected:
  bool event(QEvent* e) override;
};

#endif // VNCMACVIEW_H
