#ifndef ABSTRACTVNCVIEW_H
#define ABSTRACTVNCVIEW_H

#include <QWidget>
#include <QList>

class QMenu;
class QAction;
class QCursor;
class QLabel;
class QScreen;
class QClipboard;

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
  double devicePixelRatio() const { return m_devicePixelRatio; }
  QScreen *getCurrentScreen();

public slots:
  virtual void handleKeyPress(int keyCode, quint32 keySym);
  virtual void handleKeyRelease(int keyCode);
  virtual void setQCursor(const QCursor &cursor);
  virtual void setCursorPos(int x, int y);
  virtual void pushLEDState();
  virtual void setLEDState(unsigned int state);
  virtual void handleClipboardAnnounce(bool available);
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

protected:
  static QClipboard *m_clipboard;
  QByteArray m_geometry;
  double m_devicePixelRatio;

  quint32 m_menuKeySym;
  QMenu *m_contextMenu;
  QList<QAction*> m_actions;
  QLabel *m_overlayTip;

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
  QTimer *m_overlayTipCloseTimer;
  bool m_fullscreenEnabled;

  DownMap m_downKeySym;
  QTimer *m_mouseButtonEmulationTimer;
  int m_state;
  int m_emulatedButtonMask;
  int m_lastButtonMask;
  rfb::Point *m_lastPos;
  rfb::Point *m_origPos;

  void createContextMenu();
  void postRemoteResizeRequest();
  QList<int> fullscreenScreens();
  void filterPointerEvent(const rfb::Point &pos, int buttonMask);
  void sendAction(const rfb::Point &pos, int buttonMask, int action);
  int createButtonMask(int buttonMask);
  void handleMouseButtonEmulationTimeout();
};

#endif // ABSTRACTVNCVIEW_H
