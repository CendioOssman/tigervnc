#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <QApplication>
#include <QMenu>
#include <QPushButton>
#include <QCheckBox>
#include <QAction>
#include <QTimer>
#include <QScreen>
#include <QWindow>
#include <QDebug>
#include <QUrl>
#include <QPainter>
#include <QClipboard>
#include <QMoveEvent>
#include <QScrollBar>
#include <QAbstractEventDispatcher>

#include "rfb/ScreenSet.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/PixelBuffer.h"
#include "rfb/util.h"
#include "EmulateMB.h"
#include "BaseKeyboardHandler.h"
#include "PlatformPixelBuffer.h"
#include "appmanager.h"
#include "contextmenuactions.h"
#include "menukey.h"
#include "vncconnection.h"
#include "locale.h"
#include "i18n.h"
#undef asprintf
#include "parameters.h"
#include "vncwindow.h"
#include "abstractvncview.h"
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

QClipboard *QAbstractVNCView::clipboard_ = nullptr;

QAbstractVNCView::QAbstractVNCView(QWidget *parent, Qt::WindowFlags f)
 : QWidget(parent, f)
 , menuKeySym_(XK_F8)
 , contextMenu_(nullptr)
 , pendingServerClipboard_(false)
 , pendingClientClipboard_(false)
 , clipboardSource_(0)
 , mouseGrabbed_(false)
 , mouseButtonEmulationTimer_(new QTimer)
 , mbemu_(new EmulateMB(mouseButtonEmulationTimer_))
 , lastPointerPos_(new rfb::Point)
 , lastButtonMask_(0)
 , mousePointerTimer_(new QTimer)
 , delayedInitializeTimer_(new QTimer)
 , toastTimer_(new QTimer(this))
#ifdef QT_DEBUG
 , fpsTimer(this)
#endif
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);

  if (!clipboard_) {
    clipboard_ = QGuiApplication::clipboard();
    connect(clipboard_, &QClipboard::dataChanged, this, []() {
      if (!ViewerConfig::config()->sendClipboard()) {
        return;
      }
      //qDebug() << "QClipboard::dataChanged: owns=" << clipboard_->ownsClipboard() << ", text=" << clipboard_->text();
      if (!clipboard_->ownsClipboard()) {
        AppManager::instance()->connection()->announceClipboard(true);
      }
    });
  }
  setContentsMargins(0, 0, 0, 0);

  delayedInitializeTimer_->setInterval(1000);
  delayedInitializeTimer_->setSingleShot(true);
  connect(delayedInitializeTimer_, &QTimer::timeout, this, [this]() {
    AppManager::instance()->connection()->refreshFramebuffer();
    emit delayedInitialized();
  });
  delayedInitializeTimer_->start();

  mouseButtonEmulationTimer_->setInterval(50);
  mouseButtonEmulationTimer_->setSingleShot(true);
  connect(mouseButtonEmulationTimer_, &QTimer::timeout, this, &QAbstractVNCView::handleMouseButtonEmulationTimeout);

  mousePointerTimer_->setInterval(ViewerConfig::config()->pointerEventInterval());
  mousePointerTimer_->setSingleShot(true);
  connect(mousePointerTimer_, &QTimer::timeout, this, [this]() {
    mbemu_->filterPointerEvent(*lastPointerPos_, lastButtonMask_);
  });

  connect(AppManager::instance()->connection(), &QVNCConnection::cursorChanged, this, &QAbstractVNCView::setCursor, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::cursorPositionChanged, this, &QAbstractVNCView::setCursorPos, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::clipboardDataReceived, this, &QAbstractVNCView::handleClipboardData, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::bellRequested, this, &QAbstractVNCView::bell, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::refreshFramebufferEnded, this, &QAbstractVNCView::updateWindow, Qt::QueuedConnection);
  connect(AppManager::instance(), &AppManager::refreshRequested, this, &QAbstractVNCView::updateWindow, Qt::QueuedConnection);

  toastTimer_->setInterval(5000);
  toastTimer_->setSingleShot(true);
  connect(toastTimer_, &QTimer::timeout, this, &QAbstractVNCView::hideToast);
  connect(this, &QAbstractVNCView::delayedInitialized, this, &QAbstractVNCView::showToast);

#ifdef QT_DEBUG
  gettimeofday(&fpsLast, NULL);
  fpsTimer.start(5000);
#endif

  connect(this, &QAbstractVNCView::bufferResized, this, [=](){
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    qDebug() << "bufferResized" << pixmap.size() << size();
    repaint();
  }, Qt::QueuedConnection);
}

QAbstractVNCView::~QAbstractVNCView()
{
  for (QAction *&action: actions_) {
    delete action;
  }
  delete contextMenu_;
  delete delayedInitializeTimer_;
  delete mouseButtonEmulationTimer_;
  delete lastPointerPos_;
  delete mousePointerTimer_;
}

void QAbstractVNCView::toggleContextMenu()
{
  if(isVisibleContextMenu())
  {
    contextMenu_->hide();
  }
  else
  {
    createContextMenu();
    removeKeyboardHandler();
    contextMenu_->exec(QCursor::pos());
  }
}

void QAbstractVNCView::createContextMenu()
{
  if (!contextMenu_) {
    actions_ << new QDisconnectAction(p_("ContextMenu|", "Dis&connect"));
    actions_ << new QMenuSeparator();
    actions_ << new QFullScreenAction(p_("ContextMenu|", "&Full screen"));
    actions_ << new QMinimizeAction(p_("ContextMenu|", "Minimi&ze"));
    actions_ << new QRevertSizeAction(p_("ContextMenu|", "Resize &window to session"));
    actions_ << new QMenuSeparator();
    actions_ << new QKeyToggleAction(p_("ContextMenu|", "&Ctrl"), 0x1d, XK_Control_L);
    actions_ << new QKeyToggleAction(p_("ContextMenu|", "&Alt"), 0x38, XK_Alt_L);
    actions_ << new QMenuKeyAction(QString::asprintf(p_("ContextMenu|", "Send %s"), ViewerConfig::config()->menuKey().toStdString().c_str()));
    actions_ << new QCtrlAltDelAction(p_("ContextMenu|", "Send Ctrl-Alt-&Del"));
    actions_ << new QMenuSeparator();
    actions_ << new QRefreshAction(p_("ContextMenu|", "&Refresh screen"));
    actions_ << new QMenuSeparator();
    actions_ << new QOptionDialogAction(p_("ContextMenu|", "&Options..."));
    actions_ << new QInfoDialogAction(p_("ContextMenu|", "Connection &info..."));
    actions_ << new QAboutDialogAction(p_("ContextMenu|", "About &TigerVNC viewer..."));
    QVNCWindow *window = AppManager::instance()->window();
    contextMenu_ = new QMenu(window);
#if defined(__APPLE__)
    contextMenu_->setAttribute(Qt::WA_NativeWindow);
    cocoa_set_overlay_property(contextMenu_->winId());
#endif
    for (QAction *&action: actions_) {
      contextMenu_->addAction(action);
    }
    contextMenu_->installEventFilter(this);
    connect(contextMenu_, &QMenu::aboutToHide, this, &QAbstractVNCView::installKeyboardHandler, Qt::QueuedConnection);
  }
}

bool QAbstractVNCView::isVisibleContextMenu() const
{
  return contextMenu_ && contextMenu_->isVisible();
}

void QAbstractVNCView::sendContextMenuKey()
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  qDebug() << "QAbstractVNCView::sendContextMenuKey";
  int dummy;
  int keyCode;
  quint32 keySym;
  ::getMenuKey(&dummy, &keyCode, &keySym);
  keyboardHandler_->handleKeyPress(keyCode, keySym, true);
  keyboardHandler_->handleKeyRelease(keyCode);
  contextMenu_->hide();
}

void QAbstractVNCView::sendCtrlAltDel()
{
  keyboardHandler_->handleKeyPress(0x1d, XK_Control_L);
  keyboardHandler_->handleKeyPress(0x38, XK_Alt_L);
  keyboardHandler_->handleKeyPress(0xd3, XK_Delete);
  keyboardHandler_->handleKeyRelease(0xd3);
  keyboardHandler_->handleKeyRelease(0x38);
  keyboardHandler_->handleKeyRelease(0x1d);
}

void QAbstractVNCView::toggleKey(bool toggle, int keyCode, quint32 keySym)
{
  if (toggle) {
    keyboardHandler_->handleKeyPress(keyCode, keySym);
  }
  else {
    keyboardHandler_->handleKeyRelease(keyCode);
  }
  keyboardHandler_->setMenuKeyStatus(keySym, toggle);
}

// As QMenu eventFilter
bool QAbstractVNCView::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *e = static_cast<QKeyEvent *>(event);
    if (isVisibleContextMenu()) {
      if (QKeySequence(e->key()).toString() == ViewerConfig::config()->menuKey()) {
        toggleContextMenu();
        return true;
      }
    }
  }
  return QWidget::eventFilter(obj, event);
}

void QAbstractVNCView::initKeyboardHandler()
{
    installKeyboardHandler();
    connect(AppManager::instance(), &AppManager::vncWindowClosed, this, &QAbstractVNCView::removeKeyboardHandler, Qt::QueuedConnection);
    connect(AppManager::instance()->connection(), &QVNCConnection::ledStateChanged, keyboardHandler_, &BaseKeyboardHandler::setLEDState, Qt::QueuedConnection);
    connect(keyboardHandler_, &BaseKeyboardHandler::contextMenuKeyPressed, this, &QAbstractVNCView::toggleContextMenu, Qt::QueuedConnection);
}

void QAbstractVNCView::installKeyboardHandler()
{
    QAbstractEventDispatcher::instance()->installNativeEventFilter(keyboardHandler_);
}

void QAbstractVNCView::removeKeyboardHandler()
{
    QAbstractEventDispatcher::instance()->removeNativeEventFilter(keyboardHandler_);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
QScreen *QAbstractVNCView::screen() const
{
  // FIXME: check best overlap
  return qApp->screens()[0];
}
#endif

void QAbstractVNCView::resetKeyboard()
{
  if(keyboardHandler_)
    keyboardHandler_->resetKeyboard();
}

void QAbstractVNCView::setCursorPos(int, int)
{
}

void QAbstractVNCView::handleClipboardData(const char *data)
{
  vlog.debug("Got clipboard data (%d bytes)", (int)strlen(data));
  clipboard_->setText(data);
}

void QAbstractVNCView::maybeGrabKeyboard()
{
  QVNCWindow *window = AppManager::instance()->window();
  if (ViewerConfig::config()->fullscreenSystemKeys() && window->allowKeyboardGrab() && hasFocus()) {
    grabKeyboard();
  }
}

void QAbstractVNCView::grabKeyboard()
{
  keyboardHandler_->grabKeyboard();

  QPoint gpos = QCursor::pos();
  QPoint lpos = mapFromGlobal(gpos);
  QRect r = rect();
  if (r.contains(lpos)) {
    grabPointer();
  }
}

void QAbstractVNCView::ungrabKeyboard()
{
    if(keyboardHandler_)
        keyboardHandler_->ungrabKeyboard();
}

void QAbstractVNCView::grabPointer()
{
  setMouseTracking(true);
  mouseGrabbed_ = true;
}

void QAbstractVNCView::ungrabPointer()
{
  setMouseTracking(false);
  mouseGrabbed_ = false;
}

void QAbstractVNCView::showToast()
{
    qDebug() << "QAbstractVNCView::showToast" << toastGeometry();
    toastTimer_->start();
    damage += toastGeometry(); // xTODO
    update(localRectAdjust(damage.boundingRect()));
}

void QAbstractVNCView::hideToast()
{
    qDebug() << "QAbstractVNCView::hideToast";
    toastTimer_->stop();
    damage += toastGeometry(); // xTODO
    update(localRectAdjust(damage.boundingRect()));
}

QFont QAbstractVNCView::toastFont() const
{
    QFont f;
    f.setBold(true);
    f.setPixelSize(14);
    return f;
}

QString QAbstractVNCView::toastText() const
{
    return QString::asprintf(_("Press %s to open the context menu"), ViewerConfig::config()->menuKey().toStdString().c_str());
}

QRect QAbstractVNCView::toastGeometry() const
{
    QFontMetrics fm(toastFont());
    int b = 8;
    QRect r = fm.boundingRect(toastText()).adjusted(-2*b,-2*b,2*b,2*b);

    int x = (width() - r.width()) / 2;
    int y = 50;
    return QRect(QPoint(x, y), r.size());
}

QRect QAbstractVNCView::localRectAdjust(QRect r)
{
    return r.adjusted((width() - pixmap.width())/2,
                      (height() - pixmap.height())/2,
                      (width() - pixmap.width())/2,
                      (height() - pixmap.height())/2);
}

QRect QAbstractVNCView::remoteRectAdjust(QRect r)
{
    return r.adjusted(-(width() - pixmap.width())/2,
                      -(height() - pixmap.height())/2,
                      -(width() - pixmap.width())/2,
                      -(height() - pixmap.height())/2);
}

rfb::Point QAbstractVNCView::remotePointAdjust(const rfb::Point &pos)
{
    return rfb::Point(pos.x - (width() - pixmap.width())/2, pos.y - (height() - pixmap.height())/2);
}

// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.
void QAbstractVNCView::updateWindow()
{
  // copied from DesktopWindow.cxx.
  QVNCConnection *cc = AppManager::instance()->connection();
  if (firstUpdate_) {
    if (cc->server()->supportsSetDesktopSize) {
      emit remoteResizeRequest();
    }
    firstUpdate_ = false;
  }

  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
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

void QAbstractVNCView::paintEvent(QPaintEvent *event)
{
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());

  if ((framebuffer->width() != pixmap.width()) ||
      (framebuffer->height() != pixmap.height())) {
    pixmap = QPixmap(framebuffer->width(), framebuffer->height());
    damage = QRegion(0, 0, pixmap.width(), pixmap.height());
    resize(pixmap.width(), pixmap.height());
    emit bufferResized();
  }

  if (!damage.isEmpty()) {
    QPainter pixmapPainter(&pixmap);
    const uint8_t *data;
    int stride;
    QRect bounds = damage.boundingRect();
    int x = bounds.x();
    int y = bounds.y();
    int w = bounds.width();
    int h = bounds.height();
    rfb::Rect rfbrect(x, y, x+w, y+h);

    data = framebuffer->getBuffer(rfbrect, &stride);
    QImage image(data, w, h, stride*4, QImage::Format_RGB32);

    pixmapPainter.drawImage(bounds, image);
    damage = QRegion();
  }

  QPainter painter(this);
  QRect r = event->rect();

  painter.drawPixmap(r, pixmap, remoteRectAdjust(r));

  if(toastTimer_->isActive())
  {
    painter.setFont(toastFont());
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor("#96101010"));
    painter.drawRoundedRect(toastGeometry(), 15, 15, Qt::AbsoluteSize);
    QPen p;
    p.setColor("#e0ffffff");
    painter.setPen(p);
    painter.drawText(toastGeometry(), toastText(), QTextOption(Qt::AlignCenter));
  }

#ifdef QT_DEBUG
  fpsCounter++;
  painter.setFont(toastFont());
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
bool QAbstractVNCView::handleTimeout(rfb::Timer* t)
{
    struct timeval now;
    int            count;

    gettimeofday(&now, NULL);
    count = fpsCounter;

    fpsValue = int(count * 1000.0 / rfb::msSince(&fpsLast));

    vlog.info("%d frames in %g seconds = %d FPS",
              count,
              rfb::msSince(&fpsLast) / 1000.0,
              fpsValue);

    fpsCounter -= count;
    fpsLast = now;

    damage += fpsRect;
    update(damage);

    return true;
}
#endif

void QAbstractVNCView::getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask, int& wheelMask)
{
    buttonMask = 0;
    wheelMask  = 0;
    if (event->buttons() & Qt::LeftButton)
    {
        buttonMask |= 1;
    }
    if (event->buttons() & Qt::MiddleButton)
    {
        buttonMask |= 2;
    }
    if (event->buttons() & Qt::RightButton)
    {
        buttonMask |= 4;
    }
    if (event->buttons() & Qt::XButton1)
    {
        wheelMask |= 32;
    }
    if (event->buttons() & Qt::XButton2)
    {
        wheelMask |= 64;
    }

    x = event->x();
    y = event->y();
}

void QAbstractVNCView::getMouseProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask)
{
    buttonMask = 0;
    wheelMask  = 0;
    if (event->buttons() & Qt::LeftButton)
    {
        buttonMask |= 1;
    }
    if (event->buttons() & Qt::MiddleButton)
    {
        buttonMask |= 2;
    }
    if (event->buttons() & Qt::RightButton)
    {
        buttonMask |= 4;
    }
    if (event->buttons() & Qt::XButton1)
    {
        wheelMask |= 32;
    }
    if (event->buttons() & Qt::XButton2)
    {
        wheelMask |= 64;
    }
    if (event->delta() > 0)
    {
        wheelMask |= 8;
    }
    if (event->delta() < 0)
    {
        wheelMask |= 16;
    }

    x = event->x();
    y = event->y();
}

void QAbstractVNCView::mouseMoveEvent(QMouseEvent* event)
{
    // qDebug() << "QuickVNCItem::mousePressEvent" << event->x() << event->y();
    grabPointer();
    maybeGrabKeyboard();
    int x, y, buttonMask, wheelMask;
    getMouseProperties(event, x, y, buttonMask, wheelMask);
    filterPointerEvent(rfb::Point(x, y), buttonMask | wheelMask);
}

void QAbstractVNCView::mousePressEvent(QMouseEvent* event)
{
    qDebug() << "QuickVNCItem::mousePressEvent";

    if (ViewerConfig::config()->viewOnly())
    {
        return;
    }

    setFocus(Qt::FocusReason::MouseFocusReason);

    int x, y, buttonMask, wheelMask;
    getMouseProperties(event, x, y, buttonMask, wheelMask);
    filterPointerEvent(rfb::Point(x, y), buttonMask);

    grabPointer();
    maybeGrabKeyboard();
}

void QAbstractVNCView::mouseReleaseEvent(QMouseEvent* event)
{
    qDebug() << "QuickVNCItem::mouseReleaseEvent";

    if (ViewerConfig::config()->viewOnly())
    {
        return;
    }

    setFocus(Qt::FocusReason::MouseFocusReason);

    int x, y, buttonMask, wheelMask;
    getMouseProperties(event, x, y, buttonMask, wheelMask);
    filterPointerEvent(rfb::Point(x, y), buttonMask);

    grabPointer();
    maybeGrabKeyboard();
}

void QAbstractVNCView::wheelEvent(QWheelEvent* event)
{
    qDebug() << "QuickVNCItem::wheelEvent";

    int x, y, buttonMask, wheelMask;
    getMouseProperties(event, x, y, buttonMask, wheelMask);
    if (wheelMask) {
        filterPointerEvent(rfb::Point(x, y), buttonMask | wheelMask);
    }
    filterPointerEvent(rfb::Point(x, y), buttonMask);
    event->accept();
}

void QAbstractVNCView::focusInEvent(QFocusEvent *event)
{
    qDebug() << "QVNCWinView::focusInEvent";
    if(keyboardHandler_)
    {
        maybeGrabKeyboard();

        //flushPendingClipboard();

        // We may have gotten our lock keys out of sync with the server
        // whilst we didn't have focus. Try to sort this out.
        keyboardHandler_->pushLEDState();

        // Resend Ctrl/Alt if needed
        if (keyboardHandler_->menuCtrlKey()) {
            keyboardHandler_->handleKeyPress(0x1d, XK_Control_L);
        }
        if (keyboardHandler_->menuAltKey()) {
            keyboardHandler_->handleKeyPress(0x38, XK_Alt_L);
        }
    }
    QWidget::focusInEvent(event);
}

void QAbstractVNCView::focusOutEvent(QFocusEvent *event)
{
    qDebug() << "QVNCWinView::focusOutEvent";
    if (ViewerConfig::config()->fullscreenSystemKeys()) {
        ungrabKeyboard();
    }
    // We won't get more key events, so reset our knowledge about keys
    resetKeyboard();
    QWidget::focusOutEvent(event);
}

void QAbstractVNCView::resizeEvent(QResizeEvent* event)
{
    QVNCWindow *window = AppManager::instance()->window();
    QSize vsize = window->viewport()->size();
    qDebug() << "QAbstractVNCView::resizeEvent: w=" << event->size().width() << ", h=" << event->size().height() << ", viewport=" << vsize;

    // Try to get the remote size to match our window size, provided
    // the following conditions are true:
    //
    // a) The user has this feature turned on
    // b) The server supports it
    // c) We're not still waiting for startup fullscreen to kick in
    //
    QVNCConnection *cc = AppManager::instance()->connection();
    if (!firstUpdate_ && ViewerConfig::config()->remoteResize() && cc->server()->supportsSetDesktopSize) {
        emit remoteResizeRequest();
    }
    // Some systems require a grab after the window size has been changed.
    // Otherwise they might hold on to displays, resulting in them being unusable.
    grabPointer();
    maybeGrabKeyboard();
}

void QAbstractVNCView::filterPointerEvent(const rfb::Point& pos, int mask)
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  bool instantPosting = ViewerConfig::config()->pointerEventInterval() == 0 || (mask != lastButtonMask_);
  *lastPointerPos_ = remotePointAdjust(pos);
  lastButtonMask_ = mask;
  if (instantPosting) {
    mbemu_->filterPointerEvent(*lastPointerPos_, lastButtonMask_);
  }
  else {
    if (!mousePointerTimer_->isActive())
      mousePointerTimer_->start();
  }
}

void QAbstractVNCView::handleMouseButtonEmulationTimeout()
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  mbemu_->handleTimeout();
}
