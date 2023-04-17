#ifndef ABSTRACTVNCVIEW_H
#define ABSTRACTVNCVIEW_H

#include <QWidget>
#include <QScrollArea>
#include <QList>
#include <QLabel>

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

namespace rfb {
  struct Point;
}

using DownMap = std::map<int, quint32>;

class QAbstractVNCView : public QWidget
{
  Q_OBJECT

public:
  QAbstractVNCView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QAbstractVNCView();
  virtual void resize(int width, int height);
  void popupContextMenu();
  virtual qulonglong nativeWindowHandle() const;
  virtual bool hasViewFocus() const { return true; }
  bool hasFocus() const { return hasViewFocus() || QWidget::hasFocus() || isActiveWindow(); }
  double devicePixelRatio() const { return devicePixelRatio_; }
  virtual double effectiveDevicePixelRatio(QScreen *screen = nullptr) const;
  QScreen *getCurrentScreen();
  QClipboard *clipboard() const { return clipboard_; }
  virtual QRect getExtendedFrameProperties();
  bool isVisibleContextMenu() const;
  void sendContextMenuKey();
  void setMenuKeyStatus(quint32 keysym, bool checked);
  virtual void disableIM();
  virtual void enableIM();
  virtual void resetKeyboard();
  virtual void handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false);
  virtual void handleKeyRelease(int keyCode);
  virtual void dim(bool enabled) {}

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
  QScreen *screen() const;
#endif

public slots:
  virtual void setQCursor(const QCursor &cursor);
  virtual void setCursorPos(int x, int y);
  virtual void pushLEDState();
  virtual void setLEDState(unsigned int state);
  virtual void handleClipboardData(const char* data);
  virtual void maybeGrabKeyboard();
  virtual void grabKeyboard();
  virtual void ungrabKeyboard();
  virtual void grabPointer();
  virtual void ungrabPointer();
  virtual bool isFullscreenEnabled();
  virtual void bell();
  virtual void remoteResize(int width, int height);
  virtual void updateWindow();
  virtual void handleDesktopSize();
  virtual void fullscreen(bool enabled);
  virtual void moveView(int x, int y);
  void postRemoteResizeRequest();

signals:
  void fullscreenChanged(bool enabled);
  void delayedInitialized();

protected:
  static QClipboard *clipboard_;
  QByteArray geometry_;
  double devicePixelRatio_;

  quint32 menuKeySym_;
  QMenu *contextMenu_;
  QList<QAction*> actions_;

  bool firstLEDState_;
  bool pendingServerClipboard_;
  bool pendingClientClipboard_;
  int clipboardSource_;
  bool firstUpdate_;
  bool delayedFullscreen_;
  bool delayedDesktopSize_;
  bool keyboardGrabbed_;
  bool mouseGrabbed_;

  QTimer *resizeTimer_;
  QTimer *delayedInitializeTimer_;
  bool fullscreenEnabled_;
  bool pendingFullscreen_;

  DownMap downKeySym_;
  QTimer *mouseButtonEmulationTimer_;
  EmulateMB *mbemu_;
  int fw_;
  int fh_;
  int fxmin_;
  int fymin_;
  QScreen *fscreen_;

  rfb::Point *lastPointerPos_;
  int lastButtonMask_;
  QTimer *mousePointerTimer_;
  bool menuCtrlKey_;
  bool menuAltKey_;

  void createContextMenu();
  QList<int> fullscreenScreens();
  void filterPointerEvent(const rfb::Point &pos, int buttonMask);
  void handleMouseButtonEmulationTimeout();
  void sendPointerEvent(const rfb::Point& pos, int buttonMask);
  virtual bool bypassWMHintingEnabled() const { return false; }
  virtual void setWindowManager() {}
  virtual void fullscreenOnCurrentDisplay();
  virtual void fullscreenOnSelectedDisplay(QScreen *screen, int vx, int vy, int vwidth, int vheight);
  virtual void fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight);
  virtual void exitFullscreen();
  bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // ABSTRACTVNCVIEW_H
