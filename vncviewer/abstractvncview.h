#ifndef ABSTRACTVNCVIEW_H
#define ABSTRACTVNCVIEW_H

#include <QWidget>
#include <QScrollArea>
#include <QList>
#include <QLabel>
#include "EmulateMB.h"

class QMenu;
class QAction;
class QCursor;
class QLabel;
class QScreen;
class QClipboard;
class QMoveEvent;

namespace rfb {
  struct Point;
}

using DownMap = std::map<int, quint32>;

class QMBEmu : public EmulateMB
{
public:
  QMBEmu(QTimer *);
  ~QMBEmu();

protected:
  virtual void sendPointerEvent(const rfb::Point& pos, int buttonMask);
};

class QVNCToast : public QLabel
{
  Q_OBJECT
public:
  QVNCToast(QWidget *parent = nullptr);
  virtual ~QVNCToast();
  void show();

private:
  QTimer *m_closeTimer;
};

class QVNCWindow : public QScrollArea
{
  Q_OBJECT
public:
  QVNCWindow(QWidget *parent = nullptr);
  virtual ~QVNCWindow();

public slots:
  void popupToast();

protected:
  void moveEvent(QMoveEvent *e) override;

private:
  QVNCToast *m_toast;
};

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
  double devicePixelRatio() const { return m_devicePixelRatio; }
  QScreen *getCurrentScreen();
  QClipboard *clipboard() const { return m_clipboard; }

public slots:
  virtual void handleKeyPress(int keyCode, quint32 keySym);
  virtual void handleKeyRelease(int keyCode);
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
  virtual void popupToast();

signals:
  void fullscreenChanged(bool enabled);
  void delayedInitialized();

protected:
  static QClipboard *m_clipboard;
  QVNCToast *m_toast;
  QByteArray m_geometry;
  double m_devicePixelRatio;

  quint32 m_menuKeySym;
  QMenu *m_contextMenu;
  QList<QAction*> m_actions;

  bool m_firstLEDState;
  bool m_pendingServerClipboard;
  bool m_pendingClientClipboard;
  int m_clipboardSource;
  bool m_firstUpdate;
  bool m_delayedFullscreen;
  bool m_delayedDesktopSize;
  bool m_keyboardGrabbed;
  bool m_mouseGrabbed;

  QTimer *m_resizeTimer;
  QTimer *m_delayedInitializeTimer;
  bool m_fullscreenEnabled;
  bool m_pendingFullscreen;

  DownMap m_downKeySym;
  QTimer *m_mouseButtonEmulationTimer;
  QMBEmu *m_mbemu;
////  int m_state;
////  int m_emulatedButtonMask;
////  int m_lastButtonMask;
////  rfb::Point *m_lastPos;
////  rfb::Point *m_origPos;

  void createContextMenu();
  void postRemoteResizeRequest();
  QList<int> fullscreenScreens();
  void filterPointerEvent(const rfb::Point &pos, int buttonMask);
////  void sendAction(const rfb::Point &pos, int buttonMask, int action);
////  int createButtonMask(int buttonMask);
  void handleMouseButtonEmulationTimeout();
  void moveEvent(QMoveEvent *e) override;
  void sendPointerEvent(const rfb::Point& pos, int buttonMask);
  virtual bool bypassWMHintingEnabled() const { return false; }
};

#endif // ABSTRACTVNCVIEW_H
