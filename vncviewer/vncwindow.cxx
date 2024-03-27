#include "vncwindow.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QMoveEvent>
#include <QResizeEvent>
#include <QGestureEvent>
#include <QPainter>
#include <QStyleFactory>
#include <QScrollBar>
#include <QDebug>
#include <QTimer>
#include <QWindow>
#include <QScreen>
#include <QApplication>
#include "rfb/LogWriter.h"
#include "parameters.h"
#include "i18n.h"
#include "appmanager.h"
#include "abstractvncview.h"
#include "parameters.h"
#undef asprintf
#if defined(WIN32)
#include <windows.h>
#endif

static rfb::LogWriter vlog("VNCWindow");

QVNCWindow::QVNCWindow(QWidget *parent)
    : QScrollArea(parent)
    , resizeTimer_(new QTimer(this))
    , devicePixelRatio_(devicePixelRatioF())
{
    setAttribute(Qt::WA_InputMethodTransparent);
    setAttribute(Qt::WA_NativeWindow);
    setFocusPolicy(Qt::StrongFocus);

    setWidgetResizable(ViewerConfig::config()->remoteResize());
    setContentsMargins(0, 0, 0, 0);
    setViewportMargins(0, 0, 0, 0);
    setFrameStyle(QFrame::NoFrame);
    setLineWidth(0);

    QPalette p(palette());
    p.setColor(QPalette::Window, QColor::fromRgb(40, 40, 40));
    setPalette(p);
    setBackgroundRole(QPalette::Window);
    horizontalScrollBar()->setStyle(QStyleFactory::create("Fusion"));
    verticalScrollBar()->setStyle(QStyleFactory::create("Fusion"));

    setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Support for -geometry option. Note that although we do support
    // negative coordinates, we do not support -XOFF-YOFF (ie
    // coordinates relative to the right edge / bottom edge) at this
    // time.
    int geom_x = 0, geom_y = 0;
    if (!ViewerConfig::config()->geometry().isEmpty()) {
        int nfields = sscanf((const char*)ViewerConfig::config()->geometry().toStdString().c_str(), "+%d+%d", &geom_x, &geom_y);
        if (nfields != 2) {
            int geom_w, geom_h;
            nfields = sscanf((const char*)ViewerConfig::config()->geometry().toStdString().c_str(), "%dx%d+%d+%d", &geom_w, &geom_h, &geom_x, &geom_y);
            if (nfields != 4) {
                vlog.error(_("Invalid geometry specified!"));
            }
        }
        if (nfields == 2 || nfields == 4) {
            move(geom_x, geom_y);
        }
    }

    connect(this, &QVNCWindow::fullscreenChanged, this, [this](bool enabled) {
            setHorizontalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
            setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
        }, Qt::QueuedConnection);

    resizeTimer_->setInterval(100); // <-- DesktopWindow::resize(int x, int y, int w, int h)
    resizeTimer_->setSingleShot(true);
    connect(resizeTimer_, &QTimer::timeout, this, &QVNCWindow::handleDesktopSize);
}

QVNCWindow::~QVNCWindow()
{
}

void QVNCWindow::moveEvent(QMoveEvent *e)
{
    QScrollArea::moveEvent(e);
}

void QVNCWindow::resizeEvent(QResizeEvent *e)
{
    qDebug() << "QVNCWindow::resizeEvent: w=" << e->size().width() << ", h=" << e->size().height() << ", widgetResizable=" << widgetResizable();
    if (ViewerConfig::config()->remoteResize()) {
        QSize size = e->size();
        widget()->resize(size.width(), size.height());
    }
    else {
        scrollContentsBy(0, 0);
        updateScrollbars();
    }
    QScrollArea::resizeEvent(e);
}

void QVNCWindow::updateScrollbars()
{
    if (widget()->width() > width()) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    } else {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    if (widget()->height() > height()) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    } else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

QList<int> QVNCWindow::fullscreenScreens() const
{
    QApplication *app = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
    QList<int> applicableScreens;
    if (ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSAll) {
        for (int i = 0; i < screens.length(); i++) {
            applicableScreens << i;
        }
    }
    else if (ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSSelected) {
        for (int &id : ViewerConfig::config()->selectedScreens()) {
            int i = id - 1; // Screen ID in config is 1-origin.
            if (i < screens.length()) {
                applicableScreens << i;
            }
        }
    }
    else {
        QScreen *cscreen = getCurrentScreen();
        for (int i = 0; i < screens.length(); i++) {
            if (screens[i] == cscreen) {
                applicableScreens << i;
                break;
            }
        }
    }

    return applicableScreens;
}

QScreen *QVNCWindow::getCurrentScreen() const
{
    int centerX = x() + width() / 2;
    int centerY = y() + height() / 2;
    QPoint globalCursorPos = mapToGlobal(QPoint(centerX, centerY));
    //qDebug() << "QVNCWindow::getCurrentScreen: pos=" << globalCursorPos;
    QApplication *app = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
    for (QScreen *&screen : screens) {
        if (screen->geometry().contains(globalCursorPos)) {
            //qDebug() << "QVNCWindow::getCurrentScreen: found screen isPrimary=" << (screen == app->primaryScreen());
            return screen;
        }
    }
    return screens[0];
}

double QVNCWindow::effectiveDevicePixelRatio(QScreen *screen) const
{
#ifdef Q_OS_DARWIN
    return 1.0;
#endif

    if (screen) {
        return screen->devicePixelRatio();
    }
    return devicePixelRatio_;
}

void QVNCWindow::fullscreen(bool enabled)
{
    //qDebug() << "QVNCWindow::fullscreen: enabled=" << enabled;
    // TODO: Flag fullscreenEnabled_ seems have to be disabled before executing fullscreen().
    bool fullscreenEnabled0 = fullscreenEnabled_;
    fullscreenEnabled_ = false;
    pendingFullscreen_ = enabled;
    resizeTimer_->stop();
    QApplication *app = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
    if (enabled) {
        // cf. DesktopWindow::fullscreen_on()
        if (!fullscreenEnabled_) {
            geometry_ = saveGeometry();
            fscreen_ = getCurrentScreen();
        }

        auto mode = ViewerConfig::config()->fullScreenMode();
        QList<int> selectedScreens = fullscreenScreens();
        if (mode != ViewerConfig::FSCurrent && selectedScreens.length() > 0) {
            QScreen *selectedPrimaryScreen = screens[selectedScreens[0]];
            int xmin = INT_MAX;
            int ymin = INT_MAX;
            int xmax = INT_MIN;
            int ymax = INT_MIN;
            for (int &screenIndex : selectedScreens) {
                QScreen *screen = screens[screenIndex];
                QRect rect = screen->geometry();
                double dpr = effectiveDevicePixelRatio(screen);
                if (xmin > rect.x()) {
                    xmin = rect.x();
                }
                if (xmax < rect.x() + rect.width() * dpr) {
                    xmax = rect.x() + rect.width() * dpr;
                }
                if (ymin > rect.y()) {
                    ymin = rect.y();
                }
                if (ymax < rect.y() + rect.height() * dpr) {
                    ymax = rect.y() + rect.height() * dpr;
                }
            }
            int w = xmax - xmin;
            int h = ymax - ymin;
            //qDebug() << "Fullsize Geometry=" << QRect(xmin, ymin, w, h);
            // Capture the fullscreen geometry.
            fxmin_ = xmin;
            fymin_ = ymin;
            fw_ = w;
            fh_ = h;

            if (selectedScreens.length() == 1) { // Fullscreen on the selected single display.
                fullscreenOnSelectedDisplay(selectedPrimaryScreen, xmin, ymin, w, h);
            }
            else { // Fullscreen on multiple displays.
                fullscreenOnSelectedDisplays(xmin, ymin, w, h);
            }
        }
        else { // Fullscreen on the current single display.
            fullscreenOnCurrentDisplay();
        }
    }
    else { // Exit fullscreen mode.
        exitFullscreen();
    }
    fullscreenEnabled_ = enabled;
    pendingFullscreen_ = false;
    setFocus();
    activateWindow();
    raise();

    if (!enabled) {
        ViewerConfig::config()->setFullScreen(false);
    }
    if (fullscreenEnabled_ != fullscreenEnabled0) {
        emit fullscreenChanged(fullscreenEnabled_);
    }
}

void QVNCWindow::fullscreenOnCurrentDisplay()
{
    QVNCWindow *window = AppManager::instance()->window();
    QScreen *screen = getCurrentScreen();
    window->windowHandle()->setScreen(screen);
    window->showFullScreen();

    // Capture the fullscreen geometry.
    double dpr = effectiveDevicePixelRatio(screen);
    QRect vg = screen->geometry();
    fxmin_ = vg.x();
    fymin_ = vg.y();
    fw_ = vg.width() * dpr;
    fh_ = vg.height() * dpr;

    grabKeyboard();
}

void QVNCWindow::fullscreenOnSelectedDisplay(QScreen *screen, int vx, int vy, int, int)
{
    QVNCWindow *window = AppManager::instance()->window();
    window->windowHandle()->setScreen(screen);
    window->move(vx, vy);
    window->showFullScreen();
    grabKeyboard();
}

void QVNCWindow::fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight)
{
    QVNCWindow *window = AppManager::instance()->window();
    window->setWindowFlag(Qt::FramelessWindowHint, true);

    window->move(vx, vy);
    window->resize(vwidth, vheight);
    resize(vwidth, vheight);
    window->showNormal();
    grabKeyboard();
}

void QVNCWindow::exitFullscreen()
{
    QVNCWindow *window = AppManager::instance()->window();
    window->setWindowFlag(Qt::FramelessWindowHint, false);
    window->setWindowFlag(Qt::Window);
    window->showNormal();
    window->windowHandle()->setScreen(fscreen_);
    window->restoreGeometry(geometry_);
    QAbstractVNCView *view = AppManager::instance()->view();
    view->ungrabKeyboard();
}

bool QVNCWindow::allowKeyboardGrab() const
{
    return fullscreenEnabled_ || pendingFullscreen_;
}


void QVNCWindow::handleDesktopSize()
{
    double f = effectiveDevicePixelRatio();
    if (!ViewerConfig::config()->desktopSize().isEmpty()) {
        int w, h;
        // An explicit size has been requested
        if (sscanf(ViewerConfig::config()->desktopSize().toStdString().c_str(), "%dx%d", &w, &h) != 2) {
            return;
        }
        remoteResize(w * f, h * f);
        //qDebug() << "QVNCWindow::handleDesktopSize(explicit): width=" << w << ", height=" << h;
    }
    else if (ViewerConfig::config()->remoteResize()) {
        // No explicit size, but remote resizing is on so make sure it
        // matches whatever size the window ended up being
        remoteResize(width() * f, height() * f);
        //qDebug() << "QVNCWindow::handleDesktopSize(implicit): width=" << width() << ", height=" << height();
    }
}

void QVNCWindow::postRemoteResizeRequest()
{
    resizeTimer_->start();
}

void QVNCWindow::remoteResize(int w, int h)
{
    QVNCConnection *cc = AppManager::instance()->connection();
    rfb::ScreenSet layout;
    rfb::ScreenSet::const_iterator iter;
    double f = effectiveDevicePixelRatio();
    QVNCWindow *window = AppManager::instance()->window();
    if ((!fullscreenEnabled_ && !pendingFullscreen_) || (w > window->width() * f) || (h > window->height() * f)) {
        // In windowed mode (or the framebuffer is so large that we need
        // to scroll) we just report a single virtual screen that covers
        // the entire framebuffer.

        layout = cc->server()->screenLayout();

        // Not sure why we have no screens, but adding a new one should be
        // safe as there is nothing to conflict with...
        if (layout.num_screens() == 0)
            layout.add_screen(rfb::Screen());
        else if (layout.num_screens() != 1) {
            // More than one screen. Remove all but the first (which we
            // assume is the "primary").

            while (true) {
                iter = layout.begin();
                ++iter;

                if (iter == layout.end())
                    break;

                layout.remove_screen(iter->id);
            }
        }

        // Resize the remaining single screen to the complete framebuffer
        layout.begin()->dimensions.tl.x = 0;
        layout.begin()->dimensions.tl.y = 0;
        layout.begin()->dimensions.br.x = w;
        layout.begin()->dimensions.br.y = h;
    }
    else {
        uint32_t id;

        // In full screen we report all screens that are fully covered.
        rfb::Rect viewport_rect;
        viewport_rect.setXYWH(fxmin_, fymin_, fw_, fh_);

        // If we can find a matching screen in the existing set, we use
        // that, otherwise we create a brand new screen.
        //
        // FIXME: We should really track screens better so we can handle
        //        a resized one.
        //
        QApplication *app = static_cast<QApplication*>(QApplication::instance());
        QList<QScreen*> screens = app->screens();
        //    std::sort(screens.begin(), screens.end(), [](QScreen *a, QScreen *b) {
        //                return a->geometry().x() == b->geometry().x() ? (a->geometry().y() < b->geometry().y()) : (a->geometry().x() < b->geometry().x());
        //              });
        for (QScreen *&screen : screens) {
            double dpr = effectiveDevicePixelRatio(screen);
            QRect vg = screen->geometry();
            int sx = vg.x();
            int sy = vg.y();
            int sw = vg.width() * dpr;
            int sh = vg.height() * dpr;

            // Check that the screen is fully inside the framebuffer
            rfb::Rect screen_rect;
            screen_rect.setXYWH(sx, sy, sw, sh);
            if (!screen_rect.enclosed_by(viewport_rect))
                continue;

            // Adjust the coordinates so they are relative to our viewport
            sx -= viewport_rect.tl.x;
            sy -= viewport_rect.tl.y;

            // Look for perfectly matching existing screen that is not yet present in
            // in the screen layout...
            for (iter = cc->server()->screenLayout().begin(); iter != cc->server()->screenLayout().end(); ++iter) {
                if ((iter->dimensions.tl.x == sx) &&
                    (iter->dimensions.tl.y == sy) &&
                    (iter->dimensions.width() == sw) &&
                    (iter->dimensions.height() == sh) &&
                    (std::find(layout.begin(), layout.end(), *iter) == layout.end()))
                    break;
            }

            // Found it?
            if (iter != cc->server()->screenLayout().end()) {
                layout.add_screen(*iter);
                continue;
            }

            // Need to add a new one, which means we need to find an unused id
            while (true) {
                id = rand();
                for (iter = cc->server()->screenLayout().begin();
                     iter != cc->server()->screenLayout().end(); ++iter) {
                    if (iter->id == id)
                        break;
                }

                if (iter == cc->server()->screenLayout().end())
                    break;
            }

            layout.add_screen(rfb::Screen(id, sx, sy, sw, sh, 0));
        }

        // If the viewport doesn't match a physical screen, then we might
        // end up with no screens in the layout. Add a fake one...
        if (layout.num_screens() == 0)
            layout.add_screen(rfb::Screen(0, 0, 0, w, h, 0));
    }

    // Do we actually change anything?
    if ((w == cc->server()->width()) &&
        (h == cc->server()->height()) &&
        (layout == cc->server()->screenLayout()))
        return;

    vlog.debug("Requesting framebuffer resize from %dx%d to %dx%d",
               cc->server()->width(), cc->server()->height(), w, h);

    char buffer[2048];
    layout.print(buffer, sizeof(buffer));
    if (!layout.validate(w, h)) {
        vlog.error(_("Invalid screen layout computed for resize request!"));
        vlog.error("%s", buffer);
        return;
    }
    else {
        vlog.debug("%s", buffer);
    }
    qDebug() << "QVNCWindow::remoteResize: w=" << w << ", h=" << h << ", layout=" << buffer;
    emit AppManager::instance()->connection()->writeSetDesktopSize(w, h, layout);
}

void QVNCWindow::resize(int width, int height)
{
    qDebug() << "QVNCWindow::resize: w=" << width << ", h=" << height;
    QScrollArea::resize(width, height);
}

void QVNCWindow::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::WindowStateChange) {
        if (ViewerConfig::config()->remoteResize()) {
            qDebug() << "QVNCWindow::changeEvent: w=" << width() << ",h=" << height() << ",state=" << windowState() << ",oldState=" << (static_cast<QWindowStateChangeEvent*>(e))->oldState();
            widget()->resize(width(), height());
        }
    }
    QScrollArea::changeEvent(e);
}

void QVNCWindow::focusInEvent(QFocusEvent *)
{
    qDebug() << "QVNCWindow::focusInEvent";
    QAbstractVNCView *view = AppManager::instance()->view();
    if(view)
        view->maybeGrabKeyboard();
}

void QVNCWindow::focusOutEvent(QFocusEvent *)
{
    qDebug() << "QVNCWindow::focusOutEvent";
    if (ViewerConfig::config()->fullscreenSystemKeys()) {
        QAbstractVNCView *view = AppManager::instance()->view();
        if(view)
            view->ungrabKeyboard();
    }
}
