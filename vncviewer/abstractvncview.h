#ifndef ABSTRACTVNCVIEW_H
#define ABSTRACTVNCVIEW_H

#include <QLabel>
#include <QList>
#include <QScrollArea>
#include <QWidget>
#include <rfb/Timer.h>

class QMenu;
class QAction;
class QCursor;
class QLabel;
class QScreen;
class QClipboard;
class QMoveEvent;
class QGestureEvent;
class QVNCToast;
class EmulateMB;
class GestureHandler;
class BaseKeyboardHandler;

namespace rfb
{
struct Point;
}

using DownMap = std::map<int, quint32>;

class QAbstractVNCView : public QWidget
#ifdef QT_DEBUG
  ,
                         public rfb::Timer::Callback
#endif
{
  Q_OBJECT

public:
  QAbstractVNCView(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QAbstractVNCView();
  void toggleContextMenu();

  bool hasFocus() const { return QWidget::hasFocus() || isActiveWindow(); }

  QClipboard* clipboard() const { return clipboard_; }

  bool isVisibleContextMenu() const;
  void sendContextMenuKey();
  void sendCtrlAltDel();
  void toggleKey(bool toggle, int keyCode, quint32 keySym);
  void resize(int width, int height);
  virtual void resetKeyboard();

  QSize pixmapSize() const { return pixmap.size(); };

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
  QScreen* screen() const;
#endif

public slots:
  virtual void setCursorPos(int x, int y);
  virtual void handleClipboardData(const char* data);
  virtual void maybeGrabKeyboard();
  virtual void grabKeyboard();
  virtual void ungrabKeyboard();
  virtual void grabPointer();
  virtual void ungrabPointer();
  virtual void bell() = 0;
  void showToast();
  void hideToast();

signals:
  void delayedInitialized();
  void bufferResized();
  void remoteResizeRequest();

protected:
  QFont toastFont() const;
  QString toastText() const;
  QRect toastGeometry() const;
  QRect localRectAdjust(QRect r);
  QRect remoteRectAdjust(QRect r);
  rfb::Point remotePointAdjust(rfb::Point const& pos);
  void updateWindow();
  void paintEvent(QPaintEvent* event) override;
#ifdef QT_DEBUG
  bool handleTimeout(rfb::Timer* t) override;
#endif
  void getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);
  void getMouseProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

protected:
  static QClipboard* clipboard_;

  quint32 menuKeySym_;
  QMenu* contextMenu_;
  QList<QAction*> actions_;

  bool pendingServerClipboard_;
  bool pendingClientClipboard_;
  int clipboardSource_;
  bool mouseGrabbed_;

  DownMap downKeySym_;
  QTimer* mouseButtonEmulationTimer_;
  EmulateMB* mbemu_;

  rfb::Point* lastPointerPos_;
  int lastButtonMask_;
  QTimer* mousePointerTimer_;

  BaseKeyboardHandler* keyboardHandler_ = nullptr;
  void initKeyboardHandler();
  void installKeyboardHandler();
  void removeKeyboardHandler();

  void createContextMenu();
  void filterPointerEvent(const rfb::Point& pos, int buttonMask);
  void handleMouseButtonEmulationTimeout();
  void sendPointerEvent(const rfb::Point& pos, int buttonMask);

  // As QMenu eventFilter
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  bool firstUpdate_ = true;
  QTimer* delayedInitializeTimer_;

  QTimer* toastTimer_;

  QPixmap pixmap;
  QRegion damage;

#ifdef QT_DEBUG
  QAtomicInt fpsCounter;
  int fpsValue = 0;
  QRect fpsRect = {10, 10, 100, 20};
  struct timeval fpsLast;
  rfb::Timer fpsTimer;
#endif
};

#endif // ABSTRACTVNCVIEW_H
