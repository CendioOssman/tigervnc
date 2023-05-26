#ifndef VNCMACVIEW_H
#define VNCMACVIEW_H

#include <QAbstractNativeEventFilter>
#include "abstractvncview.h"

class QWindow;
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
  void handleClipboardData(const char* data) override;
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void handleKeyPress(int keyCode, quint32 keySym) override;
  void handleKeyRelease(int keyCode) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void bell() override;
  void updateWindow() override;
  void draw();

protected:
  bool event(QEvent *e) override;
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void paintEvent(QPaintEvent *event) override;
  void handleMouseButtonEvent(QMouseEvent*);
  void handleMouseWheelEvent(QWheelEvent*);
  void installNativeEventHandler();

signals:
  void message(const QString &msg, int timeout);

private:
  class MacEventFilter : public QAbstractNativeEventFilter
  {
  public:
      MacEventFilter(QVNCMacView *view);
      virtual ~MacEventFilter();
      bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

  private:
      QVNCMacView *m_view;
  };

  NSView *m_view;
  NSCursor *m_cursor;
  MacEventFilter *m_filter;
};

#endif // VNCMACVIEW_H
