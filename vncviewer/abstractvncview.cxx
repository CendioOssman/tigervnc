#include "abstractvncview.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// clang-format off
// QEvent must be included before X headers to avoid symbol comflicts.
#include <QEvent>
// QAction must be included before X headers to avoid symbol comflicts.
#include <QAction>
// clang-format on

#include "BaseKeyboardHandler.h"
#include "EmulateMB.h"
#include "PlatformPixelBuffer.h"
#include "appmanager.h"
#include "contextmenuactions.h"
#include "i18n.h"
#include "locale.h"
#include "menukey.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/util.h"
#include "vncconnection.h"
#include "clicksalternativegesture.h"
#include "clicksalternativegesturerecognizer.h"
#include "panzoomgesture.h"
#include "panzoomgesturerecognizer.h"
#include "tapdraggesture.h"
#include "tapdraggesturerecognizer.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QMenu>
#include <QMoveEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QWindow>
#include <QMimeData>
#include <QGestureRecognizer>
#include <QGesture>
#undef asprintf
#include "parameters.h"
#include "vncwindow.h"
#undef asprintf

#if defined(WIN32) || defined(__APPLE__)
#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"
#endif

#undef KeyPress

#if defined(__APPLE__)
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("VNCView");

QAbstractVNCView::QAbstractVNCView(QWidget* parent, Qt::WindowFlags f)
  : QWidget(parent, f)
  , mousePointerTimer(new QTimer(this))
  , delayedInitializeTimer(new QTimer(this))
#ifdef QT_DEBUG
  , fpsTimer(this)
#endif
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_NativeWindow, true);
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);
  setContentsMargins(0, 0, 0, 0);

  // 257
  registerGesture(new ClicksAlternativeGestureRecognizer, [=](Qt::GestureType type, QGestureEvent* event){
    if (ClicksAlternativeGesture *gesture = static_cast<ClicksAlternativeGesture *>(event->gesture(type))) {
      if (gesture->state() == Qt::GestureFinished) {
        QPoint pos = gesture->getPosition();
        keyboardHandler->handleKeyRelease(0x1d); // Prevents non handling of PanZoomGesture Finished
        if (gesture->getType() == ClicksAlternativeGesture::TwoPoints) {
          vlog.debug("Cendio Right click alternative gesture");
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 4 /* RightButton */);
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
          event->accept();
          return true;
        } else if (gesture->getType() == ClicksAlternativeGesture::ThreePoints) {
          vlog.debug("Cendio Middle click gesture");
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 2 /* MiddleButton */);
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
          event->accept();
          return true;
        }
      }
    }
    return false;
  });

  // 258
  registerGesture(new PanZoomGestureRecognizer, [=](Qt::GestureType type, QGestureEvent* event){
    static bool panZoomGesture = false;
    if (PanZoomGesture *gesture = static_cast<PanZoomGesture *>(event->gesture(type))) {
      if (gesture->state() == Qt::GestureUpdated) {
        if (gesture->getType() == PanZoomGesture::Pan) {
          QPoint pos = gesture->getPosition().toPoint();
          int wheelMask = 0;
          if (gesture->getOffsetDelta().y() > 0) {
            wheelMask |= 8;
          }
          if (gesture->getOffsetDelta().y() < 0) {
            wheelMask |= 16;
          }
          if (gesture->getOffsetDelta().x() > 0) {
            wheelMask |= 32;
          }
          if (gesture->getOffsetDelta().x() < 0) {
            wheelMask |= 64;
          }
          vlog.debug("Cendio Pan / Scroll gesture x=%f y=%f mask=%d",
                     gesture->getOffsetDelta().x(),
                     gesture->getOffsetDelta().y(),
                     wheelMask);
          if (!panZoomGesture) {
            panZoomGesture = true;
            QTimer::singleShot(100, this, [=](){
              panZoomGesture = false;
              filterPointerEvent(rfb::Point(pos.x(), pos.y()), wheelMask);
              filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
            });
          }
        }

        if (gesture->getType() == PanZoomGesture::Pinch) {
          QPoint pos = gesture->getPosition().toPoint();
          int wheelMask = 0;
          if (gesture->getScaleFactor() > 1.00) {
            wheelMask |= 8;
          }
          if (gesture->getScaleFactor() < 1.00) {
            wheelMask |= 16;
          }
          vlog.debug("Cendio Zoom gesture %d %f", wheelMask, gesture->getScaleFactor());
          keyboardHandler->handleKeyPress(0x1d, XK_Control_L);
          if (!panZoomGesture) {
            panZoomGesture = true;
            QTimer::singleShot(100, this, [=](){
              panZoomGesture = false;
              filterPointerEvent(rfb::Point(pos.x(), pos.y()), wheelMask);
              filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
            });
          }
          event->accept();
          return true;
        }

        if (gesture->getType() == PanZoomGesture::Undefined) {
          vlog.debug("Cendio UNDEFINED gesture");
        }
      } else {
        event->accept();
        keyboardHandler->handleKeyRelease(0x1d);
        return true;
      }
    }
    return false;
  });

  // 259
  registerGesture(new TapDragGestureRecognizer, [=](Qt::GestureType type, QGestureEvent* event){
    if (TapDragGesture *gesture = static_cast<TapDragGesture *>(event->gesture(type))) {
      QPoint pos = gesture->getPosition().toPoint();

      if (gesture->getType() == TapDragGesture::Drag) {
        static bool dragGesture = false;
        if (gesture->state() == Qt::GestureUpdated) {
          vlog.debug("Cendio Drag gesture");
          if (!dragGesture) {
            dragGesture = true;
            QPoint startPos = gesture->getStartPosition().toPoint();
            filterPointerEvent(rfb::Point(startPos.x(), startPos.y()), 1 /* LeftButton */);
          }
          filterPointerEvent(rfb::Point(pos.x(), pos.y()), 1 /* LeftButton */);
          event->accept();
          return true;
        } else if (gesture->state() == Qt::GestureFinished) {
          dragGesture = false;
          filterPointerEvent(rfb::Point(pos.x(), pos.y()), 0);
          event->accept();
          return true;
        }
      }

      if (gesture->getType() == TapDragGesture::TapAndHold) {
        if (gesture->state() == Qt::GestureFinished) {
          vlog.debug("Cendio Right click gesture");
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 4 /* RightButton */);
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
          event->accept();
          return true;
        }
      }

      if (gesture->getType() == TapDragGesture::Tap) {
        if(gesture->state() == Qt::GestureFinished) {
          vlog.debug("Cendio Click gesture");
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 1 /* LeftButton */);
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
          event->accept();
          return true;
        }
      }
    }
    return false;
  });

  for (auto type : gestureRecognizers.keys()) {
    grabGesture(type);
    vlog.debug("QGestureRecognizer::registerRecognizer type=%d", type);
  }

  connect(QGuiApplication::clipboard(), &QClipboard::changed, this, &QAbstractVNCView::handleClipboardChange);

  delayedInitializeTimer->setInterval(1000);
  delayedInitializeTimer->setSingleShot(true);
  connect(delayedInitializeTimer, &QTimer::timeout, this, [this]() {
    AppManager::instance()->getConnection()->refreshFramebuffer();
    emit delayedInitialized();
  });
  delayedInitializeTimer->start();

  mousePointerTimer->setInterval(::pointerEventInterval);
  mousePointerTimer->setSingleShot(true);
  connect(mousePointerTimer, &QTimer::timeout, this, [this]() {
    emit AppManager::instance()->getConnection()->writePointerEvent(lastPointerPos, lastButtonMask);
  });

  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::cursorChanged,
          this,
          &QAbstractVNCView::setCursor,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::cursorPositionChanged,
          this,
          &QAbstractVNCView::setCursorPos,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::clipboardRequested,
          this,
          &QAbstractVNCView::handleClipboardRequest,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::clipboardAnnounced,
          this,
          &QAbstractVNCView::handleClipboardAnnounce,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::clipboardDataReceived,
          this,
          &QAbstractVNCView::handleClipboardData,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::bellRequested,
          this,
          &QAbstractVNCView::bell,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::refreshFramebufferEnded,
          this,
          &QAbstractVNCView::updateWindow,
          Qt::QueuedConnection);
  connect(AppManager::instance(),
          &AppManager::refreshRequested,
          this,
          &QAbstractVNCView::updateWindow,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::framebufferResized,
          this,
          [=](int w, int h) {
            pixmap = QPixmap(w, h);
            damage = QRegion(0, 0, pixmap.width(), pixmap.height());
            vlog.debug("QAbstractVNCView::bufferResized pixmapSize=(%d, %d) size=(%d, %d)",
                       pixmap.size().width(), pixmap.size().height(), width(), height());
            emit bufferResized(width(), height(), w, h);
            resize(w, h);
          },
          Qt::QueuedConnection);

#ifdef QT_DEBUG
  gettimeofday(&fpsLast, nullptr);
  fpsTimer.start(5000);
#endif

  connect(
      this,
      &QAbstractVNCView::bufferResized,
      this,
      [=]() {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        repaint();
      },
      Qt::QueuedConnection);

  setMouseTracking(true);
}

QAbstractVNCView::~QAbstractVNCView()
{
  for (QAction*& action : contextMenuActions) {
    delete action;
  }
  delete contextMenu;
  for (auto gr : gestureRecognizers.keys()) {
    QGestureRecognizer::unregisterRecognizer(gr);
  }
}

void QAbstractVNCView::toggleContextMenu()
{
  if (isVisibleContextMenu()) {
    contextMenu->hide();
  } else {
    createContextMenu();
    contextMenu->exec(QCursor::pos());
    contextMenu->setFocus();
  }
}

void QAbstractVNCView::createContextMenu()
{
  if (!contextMenu) {
    contextMenu = new QMenu(this);
    contextMenuActions << new QDisconnectAction(p_("ContextMenu|", "Dis&connect"));
    contextMenuActions << new QMenuSeparator();
    auto fullScreenAction = new QFullScreenAction(p_("ContextMenu|", "&Full screen"));
    connect(contextMenu, &QMenu::aboutToShow, this, [=]() {
      fullScreenAction->setChecked(AppManager::instance()->getWindow()->isFullscreenEnabled());
    });
    contextMenuActions << fullScreenAction;
    contextMenuActions << new QMinimizeAction(p_("ContextMenu|", "Minimi&ze"));
    auto revertSizeAction = new QRevertSizeAction(p_("ContextMenu|", "Resize &window to session"));
    connect(contextMenu, &QMenu::aboutToShow, this, [=]() {
      revertSizeAction->setChecked(!AppManager::instance()->getWindow()->isFullscreenEnabled());
    });
    contextMenuActions << revertSizeAction;
    contextMenuActions << new QMenuSeparator();
    contextMenuActions << new QKeyToggleAction(p_("ContextMenu|", "&Ctrl"), 0x1d, XK_Control_L);
    contextMenuActions << new QKeyToggleAction(p_("ContextMenu|", "&Alt"), 0x38, XK_Alt_L);
    auto menuKeyAction = new QMenuKeyAction();
    contextMenuActions << menuKeyAction;
    connect(contextMenu, &QMenu::aboutToShow, this, [=]() {
      menuKeyAction->setText(QString::asprintf(p_("ContextMenu|", "Send %s"), ::menuKey.getValueStr().c_str()));
    });
    contextMenuActions << new QCtrlAltDelAction(p_("ContextMenu|", "Send Ctrl-Alt-&Del"));
    contextMenuActions << new QMenuSeparator();
    contextMenuActions << new QRefreshAction(p_("ContextMenu|", "&Refresh screen"));
    contextMenuActions << new QMenuSeparator();
    contextMenuActions << new QOptionDialogAction(p_("ContextMenu|", "&Options..."));
    contextMenuActions << new QInfoDialogAction(p_("ContextMenu|", "Connection &info..."));
    contextMenuActions << new QAboutDialogAction(p_("ContextMenu|", "About &TigerVNC viewer..."));
    for (QAction*& action : contextMenuActions) {
      contextMenu->addAction(action);
    }
    contextMenu->installEventFilter(this);
  }
}

bool QAbstractVNCView::isVisibleContextMenu() const
{
  return contextMenu && contextMenu->isVisible();
}

void QAbstractVNCView::sendContextMenuKey()
{
  vlog.debug("QAbstractVNCView::sendContextMenuKey");
  if (::viewOnly) {
    return;
  }
  int keyCode;
  quint32 keySym;
  ::getMenuKey(&keyCode, &keySym);
  keyboardHandler->handleKeyPress(keyCode, keySym, true);
  keyboardHandler->handleKeyRelease(keyCode);
  contextMenu->hide();
}

void QAbstractVNCView::sendCtrlAltDel()
{
  keyboardHandler->handleKeyPress(0x1d, XK_Control_L);
  keyboardHandler->handleKeyPress(0x38, XK_Alt_L);
  keyboardHandler->handleKeyPress(0xd3, XK_Delete);
  keyboardHandler->handleKeyRelease(0xd3);
  keyboardHandler->handleKeyRelease(0x38);
  keyboardHandler->handleKeyRelease(0x1d);
}

void QAbstractVNCView::toggleKey(bool toggle, int keyCode, quint32 keySym)
{
  if (toggle) {
    keyboardHandler->handleKeyPress(keyCode, keySym);
  } else {
    keyboardHandler->handleKeyRelease(keyCode);
  }
  keyboardHandler->setMenuKeyStatus(keySym, toggle);
}

// As QMenu eventFilter
bool QAbstractVNCView::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent* e = static_cast<QKeyEvent*>(event);
    if (isVisibleContextMenu()) {
      QString str = ::getMenuKeyQString();
      if (!str.isEmpty() && QKeySequence(e->key()).toString() == str) {
        sendContextMenuKey();
        return true;
      }
    }
  }
  return QWidget::eventFilter(obj, event);
}

void QAbstractVNCView::resize(int width, int height)
{
  vlog.debug("QAbstractVNCView::resize size=(%d, %d)", width, height);
  if (this->width() == width && this->height() == height) {
    vlog.debug("QAbstractVNCView::resize ignored");
    return;
  }
  QWidget::resize(width, height);
}

void QAbstractVNCView::initKeyboardHandler()
{
  installKeyboardHandler();
  connect(AppManager::instance(),
          &AppManager::vncWindowClosed,
          this,
          &QAbstractVNCView::removeKeyboardHandler,
          Qt::QueuedConnection);
  connect(
      AppManager::instance()->getConnection(),
      &QVNCConnection::ledStateChanged,
      this,
      [=](unsigned int state) {
        vlog.debug("QVNCConnection::ledStateChanged");
        // The first message is just considered to be the server announcing
        // support for this extension. We will push our state to sync up the
        // server when we get focus. If we already have focus we need to push
        // it here though.
        if (firstLEDState) {
          firstLEDState = false;
          if (hasFocus()) {
            vlog.debug("KeyboardHandler::pushLEDState");
            keyboardHandler->pushLEDState();
          }
        } else if (hasFocus()) {
          vlog.debug("KeyboardHandler::setLEDState");
          keyboardHandler->setLEDState(state);
        }
      },
      Qt::QueuedConnection);
  connect(keyboardHandler,
          &BaseKeyboardHandler::contextMenuKeyPressed,
          this,
          &QAbstractVNCView::toggleContextMenu,
          Qt::QueuedConnection);
}

void QAbstractVNCView::installKeyboardHandler()
{
  vlog.debug("QAbstractVNCView::installKeyboardHandler");
  QAbstractEventDispatcher::instance()->installNativeEventFilter(keyboardHandler);
}

void QAbstractVNCView::removeKeyboardHandler()
{
  vlog.debug("QAbstractVNCView::removeNativeEventFilter");
  QAbstractEventDispatcher::instance()->removeNativeEventFilter(keyboardHandler);
}

void QAbstractVNCView::resetKeyboard()
{
  if (keyboardHandler)
    keyboardHandler->resetKeyboard();
}

void QAbstractVNCView::setCursorPos(int x, int y)
{
  vlog.debug("QAbstractVNCView::setCursorPos mouseGrabbed=%d", mouseGrabbed);
  vlog.debug("QAbstractVNCView::setCursorPos keyboardGrabbed=%d", keyboardHandler->isKeyboardGrabbed());
  if (!mouseGrabbed || !keyboardHandler->isKeyboardGrabbed()) {
    // Do nothing if we do not have the mouse captured.
    return;
  }
  QPoint gp = mapToGlobal(localPointAdjust(QPoint(x, y)));
  vlog.debug("QAbstractVNCView::setCursorPos local x=%d y=%d", x, y);
  vlog.debug("QAbstractVNCView::setCursorPos screen x=%d y=%d", gp.x(), gp.y());
  x = gp.x();
  y = gp.y();
  QCursor::setPos(x, y);
}

void QAbstractVNCView::flushPendingClipboard()
{
  if (pendingServerClipboard) {
    vlog.debug("Focus regained after remote clipboard change, requesting data");
    AppManager::instance()->getConnection()->requestClipboard();
  }

  if (pendingClientClipboard) {
    vlog.debug("Focus regained after local clipboard change, notifying server");
    AppManager::instance()->getConnection()->announceClipboard(true);
  }

  pendingServerClipboard = false;
  pendingClientClipboard = false;
}

void QAbstractVNCView::handleClipboardRequest()
{
  vlog.debug("QAbstractVNCView::handleClipboardRequest: %s", pendingClientData.toStdString().c_str());
  vlog.debug("Sending clipboard data (%d bytes)", (int)pendingClientData.size());
  AppManager::instance()->getConnection()->sendClipboardData(pendingClientData);
  pendingClientData = "";
}

void QAbstractVNCView::handleClipboardChange(QClipboard::Mode mode)
{
  vlog.debug("QAbstractVNCView::handleClipboardChange: mode=%d", mode);
  vlog.debug("QAbstractVNCView::handleClipboardChange: text=%s", QGuiApplication::clipboard()->text(mode).toStdString().c_str());
  vlog.debug("QAbstractVNCView::handleClipboardChange: ownsClipboard=%d", QGuiApplication::clipboard()->ownsClipboard());
  vlog.debug("QAbstractVNCView::handleClipboardChange: hasText=%d", QGuiApplication::clipboard()->mimeData(mode)->hasText());

  if (!::sendClipboard) {
    return;
  }

#if !defined(WIN32) && !defined(__APPLE__)
  if (mode == QClipboard::Mode::Selection && !::sendPrimary) {
    return;
  }
#endif

  if(mode == QClipboard::Mode::Clipboard && QGuiApplication::clipboard()->ownsClipboard()) {
    return;
  }

  if(mode == QClipboard::Mode::Selection && QGuiApplication::clipboard()->ownsSelection()) {
    return;
  }

  if (!QGuiApplication::clipboard()->mimeData(mode)->hasText()) {
    return;
  }

#ifdef __APPLE__
  if (QGuiApplication::clipboard()->text(mode) == serverReceivedData) {
    serverReceivedData = "";
    return;
  }
#endif

  pendingServerClipboard = false;
  pendingClientData = QGuiApplication::clipboard()->text(mode);

  if (!hasFocus()) {
    vlog.debug("Local clipboard changed whilst not focused, will notify server later");
    pendingClientClipboard = true;
    // Clear any older client clipboard from the server
    AppManager::instance()->getConnection()->announceClipboard(false);
    return;
  }

  vlog.debug("Local clipboard changed, notifying server");
  AppManager::instance()->getConnection()->announceClipboard(true);
}

void QAbstractVNCView::handleClipboardAnnounce(bool available)
{
  vlog.debug("QAbstractVNCView::handleClipboardAnnounce: available=%d", available);

  if (!::acceptClipboard)
    return;

  if (!available) {
    vlog.debug("Clipboard is no longer available on server");
    pendingServerClipboard = false;
    return;
  }

  pendingClientClipboard = false;
  pendingClientData = "";

  if (!hasFocus()) {
    vlog.debug("Got notification of new clipboard on server whilst not focused, will request data later");
    pendingServerClipboard = true;
    return;
  }

  vlog.debug("Got notification of new clipboard on server, requesting data");
  QVNCConnection* cc = AppManager::instance()->getConnection();
  cc->requestClipboard();
}

void QAbstractVNCView::handleClipboardData(const char* cbdata)
{
  vlog.debug("QAbstractVNCView::handleClipboardData: %s", cbdata);
  vlog.debug("Got clipboard data (%d bytes)", (int)strlen(cbdata));
#ifdef __APPLE__
  serverReceivedData = cbdata;
#endif
  QGuiApplication::clipboard()->setText(cbdata);
#if !defined(WIN32) && !defined(__APPLE__)
  if (::setPrimary)
    QGuiApplication::clipboard()->setText(cbdata, QClipboard::Mode::Selection);
#endif
}

void QAbstractVNCView::maybeGrabKeyboard()
{
  vlog.debug("QAbstractVNCView::maybeGrabKeyboard");
  QVNCWindow* window = AppManager::instance()->getWindow();
  if (::fullscreenSystemKeys && window->allowKeyboardGrab() && hasFocus()) {
    grabKeyboard();
  }
}

void QAbstractVNCView::grabKeyboard()
{
  vlog.debug("QAbstractVNCView::grabKeyboard");
  keyboardHandler->grabKeyboard();

  QPoint gpos = QCursor::pos();
  QPoint lpos = mapFromGlobal(gpos);
  QRect r = rect();
  if (r.contains(lpos)) {
    grabPointer();
  }
}

void QAbstractVNCView::ungrabKeyboard()
{
  if (keyboardHandler)
    keyboardHandler->ungrabKeyboard();
  ungrabPointer();
}

void QAbstractVNCView::maybeGrabPointer()
{
  vlog.debug("QAbstractVNCView::maybeGrabPointer");
  QVNCWindow* window = AppManager::instance()->getWindow();
  if (::fullscreenSystemKeys && window->allowKeyboardGrab() && hasFocus()) {
    grabPointer();
  }
}

void QAbstractVNCView::grabPointer()
{
  vlog.debug("QAbstractVNCView::grabPointer");
  activateWindow();
  mouseGrabbed = true;
}

void QAbstractVNCView::ungrabPointer()
{
  mouseGrabbed = false;
}

QPoint QAbstractVNCView::localPointAdjust(QPoint p)
{
  p.rx() += (width() - pixmap.width()) / 2;
  p.ry() += (height() - pixmap.height()) / 2;
  return p;
}

QRect QAbstractVNCView::localRectAdjust(QRect r)
{
  return r.adjusted((width() - pixmap.width()) / 2,
                    (height() - pixmap.height()) / 2,
                    (width() - pixmap.width()) / 2,
                    (height() - pixmap.height()) / 2);
}

QRect QAbstractVNCView::remoteRectAdjust(QRect r)
{
  return r.adjusted(-(width() - pixmap.width()) / 2,
                    -(height() - pixmap.height()) / 2,
                    -(width() - pixmap.width()) / 2,
                    -(height() - pixmap.height()) / 2);
}

rfb::Point QAbstractVNCView::remotePointAdjust(const rfb::Point& pos)
{
  return rfb::Point(pos.x - (width() - pixmap.width()) / 2, pos.y - (height() - pixmap.height()) / 2);
}

// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.
void QAbstractVNCView::updateWindow()
{
  // copied from DesktopWindow.cxx.
  QVNCConnection* cc = AppManager::instance()->getConnection();
  if (firstUpdate) {
    if (cc->server()->supportsSetDesktopSize) {
      emit remoteResizeRequest();
    }
    firstUpdate = false;
  }

  PlatformPixelBuffer* framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect rect = framebuffer->getDamage();
  int x = rect.tl.x;
  int y = rect.tl.y;
  int w = rect.br.x - x;
  int h = rect.br.y - y;
  if (!rect.is_empty()) {
    damage += QRect(x, y, w, h);
    update(localRectAdjust(QRect(x, y, w, h)));
  }
}

void QAbstractVNCView::paintEvent(QPaintEvent* event)
{
  QVNCConnection* cc = AppManager::instance()->getConnection();
  PlatformPixelBuffer* framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());

  if ((framebuffer->width() != pixmap.width()) || (framebuffer->height() != pixmap.height())) {
    update();
    return;
  }

  if (!damage.isEmpty()) {
    QPainter pixmapPainter(&pixmap);
    const uint8_t* fbdata;
    int stride;
    QRect bounds = damage.boundingRect();
    int x = bounds.x();
    int y = bounds.y();
    int w = bounds.width();
    int h = bounds.height();
    rfb::Rect rfbrect(x, y, x + w, y + h);

    if (rfbrect.enclosed_by(framebuffer->getRect())) {
      fbdata = framebuffer->getBuffer(rfbrect, &stride);
      QImage image(fbdata, w, h, stride * 4, QImage::Format_RGB32);
#ifdef __APPLE__
      pixmapPainter.fillRect(bounds, QColor("#ff000000"));
      pixmapPainter.setCompositionMode(QPainter::CompositionMode_Plus);
#endif
      pixmapPainter.drawImage(bounds, image);
    }
    damage = QRegion();
  }

  QPainter painter(this);
  QRect r = event->rect();

  painter.drawPixmap(r, pixmap, remoteRectAdjust(r));

#ifdef QT_DEBUG
  fpsCounter++;
  QFont f;
  f.setBold(true);
  f.setPixelSize(14);
  painter.setFont(f);
  painter.setPen(Qt::NoPen);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor("#96101010"));
  painter.drawRect(fpsRect);
  QPen p;
  p.setColor("#e0ffffff");
  painter.setPen(p);
  QString text = QString("%1 fps").arg(fpsValue);
  painter.drawText(fpsRect, text, QTextOption(Qt::AlignCenter));

  painter.setBrush(Qt::NoBrush);
  painter.setPen(Qt::red);
  painter.drawRect(rect());
#endif

  setAttribute(Qt::WA_OpaquePaintEvent, true);
}

#ifdef QT_DEBUG
void QAbstractVNCView::handleTimeout(rfb::Timer* t)
{
  struct timeval now;
  int count;

  EmulateMB::handleTimeout(t);

  if (t != &fpsTimer)
    return;

  gettimeofday(&now, nullptr);
  count = fpsCounter;

  fpsValue = int(count * 1000.0 / rfb::msSince(&fpsLast));

  vlog.info("%d frames in %g seconds = %d FPS", count, rfb::msSince(&fpsLast) / 1000.0, fpsValue);

  fpsCounter -= count;
  fpsLast = now;

  damage += fpsRect;
  update(damage);

  t->repeat();
}
#endif

void QAbstractVNCView::getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask)
{
  buttonMask = 0;
  if (event->buttons() & Qt::LeftButton) {
    buttonMask |= 1;
  }
  if (event->buttons() & Qt::MiddleButton) {
    buttonMask |= 2;
  }
  if (event->buttons() & Qt::RightButton) {
    buttonMask |= 4;
  }

  x = event->x();
  y = event->y();
}

void QAbstractVNCView::getMouseWheelProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask)
{
  buttonMask = 0;
  wheelMask = 0;
  if (event->buttons() & Qt::LeftButton) {
    buttonMask |= 1;
  }
  if (event->buttons() & Qt::MiddleButton) {
    buttonMask |= 2;
  }
  if (event->buttons() & Qt::RightButton) {
    buttonMask |= 4;
  }
  if (event->angleDelta().y() > 0) {
    wheelMask |= 8;
  }
  if (event->angleDelta().y() < 0) {
    wheelMask |= 16;
  }
  if (event->angleDelta().x() > 0) {
    wheelMask |= 32;
  }
  if (event->angleDelta().x() < 0) {
    wheelMask |= 64;
  }

  x = event->position().x();
  y = event->position().y();
}

void QAbstractVNCView::mouseMoveEvent(QMouseEvent* event)
{
  if(event->source() != Qt::MouseEventNotSynthesized) {
    return;
  }

  maybeGrabKeyboard();
  int x, y, buttonMask;
  getMouseProperties(event, x, y, buttonMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask);
}

void QAbstractVNCView::mousePressEvent(QMouseEvent* event)
{
  vlog.debug("QAbstractVNCView::mousePressEvent");

  if(event->source() != Qt::MouseEventNotSynthesized) {
    vlog.debug("!MouseEventNotSynthesized");
    event->accept();
    return;
  }

  if (::viewOnly) {
    return;
  }

  setFocus(Qt::FocusReason::MouseFocusReason);

  int x, y, buttonMask;
  getMouseProperties(event, x, y, buttonMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask);

  maybeGrabKeyboard();
}

void QAbstractVNCView::mouseReleaseEvent(QMouseEvent* event)
{
  vlog.debug("QAbstractVNCView::mouseReleaseEvent");

  if(event->source() != Qt::MouseEventNotSynthesized) {
    vlog.debug("!MouseEventNotSynthesized");
    event->accept();
    return;
  }

  if (::viewOnly) {
    return;
  }

  setFocus(Qt::FocusReason::MouseFocusReason);

  int x, y, buttonMask;
  getMouseProperties(event, x, y, buttonMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask);

  maybeGrabKeyboard();
}

void QAbstractVNCView::wheelEvent(QWheelEvent* event)
{
  vlog.debug("QAbstractVNCView::wheelEvent");

  int x, y, buttonMask, wheelMask;
  getMouseWheelProperties(event, x, y, buttonMask, wheelMask);
  if (wheelMask) {
    filterPointerEvent(rfb::Point(x, y), buttonMask | wheelMask);
  }
  filterPointerEvent(rfb::Point(x, y), buttonMask);
  event->accept();
}

void QAbstractVNCView::giveKeyboardFocus()
{
  vlog.debug("QAbstractVNCView::giveKeyboardFocus");
  if (qApp->activeModalWidget()) {
    vlog.debug("QAbstractVNCView::giveKeyboardFocus activeModalWidget=%s", qApp->activeModalWidget()->metaObject()->className());
  }
  if (keyboardHandler && !qApp->activeModalWidget()) {
    installKeyboardHandler();
    maybeGrabKeyboard();

    flushPendingClipboard();

           // We may have gotten our lock keys out of sync with the server
           // whilst we didn't have focus. Try to sort this out.
    vlog.debug("KeyboardHandler::pushLEDState");
    keyboardHandler->pushLEDState();

           // Resend Ctrl/Alt if needed
    if (keyboardHandler->getMenuCtrlKey()) {
      keyboardHandler->handleKeyPress(0x1d, XK_Control_L);
    }
    if (keyboardHandler->getMenuAltKey()) {
      keyboardHandler->handleKeyPress(0x38, XK_Alt_L);
    }
  }
}

void QAbstractVNCView::focusInEvent(QFocusEvent* event)
{
  vlog.debug("QAbstractVNCView::focusInEvent");
  giveKeyboardFocus();
  QWidget::focusInEvent(event);
#ifdef __APPLE__
  vlog.debug("cocoa_update_window_level hasFocus=%d", hasFocus());
  if (hasFocus()) {
    auto window = AppManager::instance()->getWindow();
    bool shielding = ::fullscreenSystemKeys && window->allowKeyboardGrab();
    cocoa_update_window_level(window, window->isFullscreenEnabled(), shielding);
  }
#endif
}

void QAbstractVNCView::focusOutEvent(QFocusEvent* event)
{
  vlog.debug("QAbstractVNCView::focusOutEvent");
  if (::fullscreenSystemKeys) {
    ungrabKeyboard();
  }
  // We won't get more key events, so reset our knowledge about keys
  resetKeyboard();
  removeKeyboardHandler();
  QWidget::focusOutEvent(event);
#ifdef __APPLE__
  vlog.debug("cocoa_update_window_level hasFocus=%d", hasFocus());
  if (!hasFocus()) {
    auto window = AppManager::instance()->getWindow();
    cocoa_update_window_level(window, false);
  }
#endif
}

void QAbstractVNCView::resizeEvent(QResizeEvent* event)
{
  vlog.debug("QAbstractVNCView::resizeEvent size=(%d, %d)",
             event->size().width(),
             event->size().height());
  // Some systems require a grab after the window size has been changed.
  // Otherwise they might hold on to displays, resulting in them being unusable.
  maybeGrabKeyboard();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void QAbstractVNCView::enterEvent(QEvent *event)
#else
void QAbstractVNCView::enterEvent(QEnterEvent *event)
#endif
{
  vlog.debug("QAbstractVNCView::enterEvent");
  maybeGrabPointer();
  QWidget::enterEvent(event);
}

void QAbstractVNCView::leaveEvent(QEvent *event)
{
  vlog.debug("QAbstractVNCView::leaveEvent");
  ungrabPointer();
  QWidget::leaveEvent(event);
}

bool QAbstractVNCView::event(QEvent *event)
{
  switch (event->type()) {
  case QEvent::WindowActivate:
    vlog.debug("QAbstractVNCView::WindowActivate");
    break;
  case QEvent::WindowDeactivate:
    vlog.debug("QAbstractVNCView::WindowDeactivate");
    ungrabPointer();
    break;
  case QEvent::CursorChange:
    event->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    return true;
  case QEvent::Gesture:
    return gestureEvent(reinterpret_cast<QGestureEvent*>(event));
  default:
    break;
  }
  return QWidget::event(event);
}

void QAbstractVNCView::sendPointerEvent(const rfb::Point& pos, uint8_t buttonMask)
{
  if (::viewOnly) {
    return;
  }
  bool instantPosting = ::pointerEventInterval == 0 || (buttonMask != lastButtonMask);
  lastPointerPos = remotePointAdjust(pos);
  lastButtonMask = buttonMask;
  if (instantPosting) {
    emit AppManager::instance()->getConnection()->writePointerEvent(pos, buttonMask);
  } else {
    if (!mousePointerTimer->isActive()) {
      mousePointerTimer->start();
    }
  }
}

bool QAbstractVNCView::gestureEvent(QGestureEvent* event)
{
  vlog.debug("QAbstractVNCView::gestureEvent");

  if (::viewOnly) {
    return true;
  }

  static QMap<QTapGesture*, bool> tapGestures;
  for(auto g : event->gestures()) {
    vlog.debug("QAbstractVNCView::gestureEvent: %d %s",
               g->gestureType(),
               QVariant::fromValue(g->state()).toString().toStdString().c_str());
  }

  for (auto gr : gestureRecognizers) {
    if (gr.second(event))
      return true;
  }

  return true;
}

void QAbstractVNCView::registerGesture(QGestureRecognizer *gr, GestureCallbackWithType cb)
{
  auto type = QGestureRecognizer::registerRecognizer(gr);
  gestureRecognizers.insert(type, QPair<QGestureRecognizer*, GestureCallback>(gr, std::bind(cb, type, std::placeholders::_1)));
}
