#ifndef VNCMACVIEW_H
#define VNCMACVIEW_H

#include "abstractvncview.h"

class QWindow;
class QTimer;
class QLabel;
class NSView;
class NSCursor;

class QVNCMacView : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCMacView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCMacView();
  qulonglong nativeWindowHandle() const override;

public slots:
  void setQCursor(const QCursor &cursor) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
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
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

  void handleMouseButtonEvent(QMouseEvent*);
  void handleMouseWheelEvent(QWheelEvent*);

signals:
  void message(const QString &msg, int timeout);

public slots:
  void addInvalidRegion(int x0, int y0, int x1, int y1);
  void draw();

private:
  NSView *m_view;
  NSCursor *m_cursor;
  bool m_dirty;
  QTimer *m_refreshTimer;
};

#endif // VNCMACVIEW_H
