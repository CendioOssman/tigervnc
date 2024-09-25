#ifndef ABSTRACTVNCVIEW_H
#define ABSTRACTVNCVIEW_H

#include <QClipboard>
#include <QLabel>
#include <QMap>
#include <QScrollArea>
#include <QWidget>
#include <functional>

#include "rfb/Rect.h"
#include "rfb/Timer.h"

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
class QGestureRecognizer;

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

  bool isVisibleContextMenu() const;
  void sendContextMenuKey();
  void sendCtrlAltDel();
  void toggleKey(bool toggle, int keyCode, quint32 keySym);
  void resize(int width, int height);
  virtual void resetKeyboard();

  QSize pixmapSize() const { return pixmap.size(); };

public slots:
  virtual void setCursorPos(int x, int y);
  void flushPendingClipboard();
  void handleClipboardRequest();
  void handleClipboardChange(QClipboard::Mode mode);
  void handleClipboardAnnounce(bool available);
  void handleClipboardData(const char* data);
  virtual void maybeGrabKeyboard();
  virtual void grabKeyboard();
  virtual void ungrabKeyboard();
  virtual void maybeGrabPointer();
  virtual void grabPointer();
  virtual void ungrabPointer();
  virtual void bell() = 0;
  void giveKeyboardFocus();

signals:
  void delayedInitialized();
  void bufferResized(int oldW, int oldH, int w, int h);
  void remoteResizeRequest();

protected:
  QPoint localPointAdjust(QPoint p);
  QRect localRectAdjust(QRect r);
  QRect remoteRectAdjust(QRect r);
  rfb::Point remotePointAdjust(rfb::Point const& pos);
  void updateWindow();
  void paintEvent(QPaintEvent* event) override;
#ifdef QT_DEBUG
  void handleTimeout(rfb::Timer* t) override;
#endif
  void getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask);
  void getMouseWheelProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEvent* event) override;
#else
  void enterEvent(QEnterEvent* event) override;
#endif
  void leaveEvent(QEvent* event) override;
  bool event(QEvent* event) override;

protected:
  // Mouse
  bool mouseGrabbed = false;
  EmulateMB* mbemu;
  rfb::Point lastPointerPos;
  int lastButtonMask = 0;
  QTimer* mousePointerTimer;

  // Keyboard handler
  bool firstLEDState = true;
  BaseKeyboardHandler* keyboardHandler = nullptr;
  void initKeyboardHandler();
  void installKeyboardHandler();
  void removeKeyboardHandler();

  // Context menu
  QMenu* contextMenu = nullptr;
  QList<QAction*> contextMenuActions;
  void createContextMenu();
  void filterPointerEvent(const rfb::Point& pos, int buttonMask);
  // As QMenu eventFilter
  bool eventFilter(QObject* watched, QEvent* event) override;

  // Clipboard
  bool pendingServerClipboard = false;
  bool pendingClientClipboard = false;
  QString pendingClientData;
#ifdef __APPLE__
  QString serverReceivedData;
#endif

  // Gestures
  typedef std::function<bool(QGestureEvent*)> GestureCallback;
  typedef std::function<bool(Qt::GestureType, QGestureEvent*)> GestureCallbackWithType;
  QMap<Qt::GestureType, QPair<QGestureRecognizer*, GestureCallback>> gestureRecognizers;
  bool gestureEvent(QGestureEvent *event);
  void registerGesture(QGestureRecognizer* gr, GestureCallbackWithType cb);

private:
  // Initialization
  bool firstUpdate = true;
  QTimer* delayedInitializeTimer;

  // Rendering
  QPixmap pixmap;
  QRegion damage;

  // FPS debugging
#ifdef QT_DEBUG
  QAtomicInt fpsCounter;
  int fpsValue = 0;
  QRect fpsRect = {10, 10, 100, 20};
  struct timeval fpsLast;
  rfb::Timer fpsTimer;
#endif
};

#endif // ABSTRACTVNCVIEW_H
