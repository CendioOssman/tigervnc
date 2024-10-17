/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2011 D. R. Commander.  All Rights Reserved.
 * Copyright 2009-2014 Pierre Ossman for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#include "rfb/screenTypes.h"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QGuiApplication>
#include <QTimer>
#include <QCursor>
#include <QClipboard>
#include <QPixmap>
#include <QSocketNotifier>

#if !defined(__APPLE__) && !defined(WIN32)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif
#include <X11/Xlib.h>
#endif

#include <time.h>
#include "rfb/Hostname.h"
#include "rfb/LogWriter.h"
#include "rfb/fenceTypes.h"
#include "rfb/CMsgWriter.h"
#include "network/TcpSocket.h"
#include "parameters.h"
#include "PlatformPixelBuffer.h"
#include "i18n.h"
#include "appmanager.h"
#include "OptionsDialog.h"
#include "DesktopWindow.h"
#include "UserDialog.h"
#include "CConn.h"
#include "vncviewer.h"
#undef asprintf

#if !defined(Q_OS_WIN)
#include "network/UnixSocket.h"
#endif

#ifdef __APPLE__
#include "cocoa.h"
#endif

using namespace rdr;
using namespace rfb;

static LogWriter vlog("CConn");

// 8 colours (1 bit per component)
static const PixelFormat verylowColourPF(8, 3,false, true,
                                         1, 1, 1, 2, 1, 0);
// 64 colours (2 bits per component)
static const PixelFormat lowColourPF(8, 6, false, true,
                                     3, 3, 3, 4, 2, 0);
// 256 colours (2-3 bits per component)
static const PixelFormat mediumColourPF(8, 8, false, true,
                                        7, 7, 3, 5, 2, 0);

// Time new bandwidth estimates are weighted against (in ms)
static const unsigned bpsEstimateWindow = 1000;

CConn::CConn(const char* vncserver, network::Socket* socket_)
 : CConnection()
 , serverHost("")
 , serverPort(5900)
 , socket(socket_)
 , socketReadNotifier(nullptr)
 , socketWriteNotifier(nullptr)
 , desktop(nullptr)
 , updateCount(0)
 , pixelCount(0)
 , serverPF(new PixelFormat)
 , fullColourPF(new PixelFormat(32, 24, false, true, 255, 255, 255, 16, 8, 0))
 , lastServerEncoding((unsigned int)-1)
 , updateStartPos(0)
 , bpsEstimate(20000000)
 , updateTimer(this, &CConn::handleUpdateTimeout)
{
  setShared(::shared);
  
  supportsLocalCursor = true;
  supportsCursorPosition = true;
  supportsDesktopResize = true;
  supportsLEDState = true;

  QString address = vncserver;
  if (socket == nullptr) {
    try {
#ifndef Q_OS_WIN
      if (address.contains("/")) {
        socket = new network::UnixSocket(address.toStdString().c_str());
        serverHost = socket->getPeerAddress();
        vlog.info(_("Connected to socket %s"), serverHost.toStdString().c_str());
      } else
#endif
      {
        std::string shost;
        rfb::getHostAndPort(address.toStdString().c_str(), &shost, &serverPort);
        serverHost = shost.c_str();

        socket = new network::TcpSocket(shost.c_str(), serverPort);
        vlog.info(_("Connected to host %s port %d"),
                  shost.c_str(), serverPort);
      }
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection(_("Failed to connect to \"%s\":\n\n%s"),
                       address.toStdString().c_str(), e.str());
      return;
    }
  }

  setStreams(&socket->inStream(), &socket->outStream());

  socketReadNotifier = new QSocketNotifier(socket->getFd(), QSocketNotifier::Read);
  QObject::connect(socketReadNotifier, &QSocketNotifier::activated, [this](int) {
    startProcessing();
  });

  socketWriteNotifier = new QSocketNotifier(socket->getFd(), QSocketNotifier::Write);
  socketWriteNotifier->setEnabled(false);
  QObject::connect(socketWriteNotifier, &QSocketNotifier::activated, [this](int) {
    flushSocket();
  });

  initialiseProtocol();

  credential = new UserDialog;
  if (::customCompressLevel) {
    setCompressLevel(::compressLevel);
  }

  if (!::noJpeg) {
    setQualityLevel(::qualityLevel);
  }

  OptionsDialog::addCallback(handleOptions, this);
}

CConn::~CConn()
{
  close();

  OptionsDialog::removeCallback(handleOptions);

  if (desktop)
    delete desktop;

  delete serverPF;
  delete fullColourPF;

  delete socketReadNotifier;
  delete socketWriteNotifier;
  delete socket;
}

QString CConn::connectionInfo()
{
  QString infoText;
  char pfStr[100];

  infoText += QString::asprintf(_("Desktop name: %.80s"), server.name()) + "\n";
  infoText += QString::asprintf(_("Host: %.80s port: %d"), serverHost.toStdString().c_str(), serverPort) + "\n";
  infoText += QString::asprintf(_("Size: %d x %d"), server.width(), server.height()) + "\n";

  // TRANSLATORS: Will be filled in with a string describing the
  // protocol pixel format in a fairly language neutral way
  server.pf().print(pfStr, 100);
  infoText += QString::asprintf(_("Pixel format: %s"), pfStr) + "\n";

  // TRANSLATORS: Similar to the earlier "Pixel format" string
  serverPF->print(pfStr, 100);
  infoText += QString::asprintf(_("(server default %s)"), pfStr) + "\n";
  infoText += QString::asprintf(_("Requested encoding: %s"), encodingName(getPreferredEncoding())) + "\n";
  infoText += QString::asprintf(_("Last used encoding: %s"), encodingName(lastServerEncoding)) + "\n";
  infoText += QString::asprintf(_("Line speed estimate: %d kbit/s"), (int)(bpsEstimate/1000)) + "\n";
  infoText += QString::asprintf(_("Protocol version: %d.%d"), server.majorVersion, server.minorVersion) + "\n";
  infoText += QString::asprintf(_("Security method: %s"), secTypeName(securityType())) + "\n";

  return infoText;
}

unsigned CConn::getUpdateCount()
{
  return updateCount;
}

unsigned CConn::getPixelCount()
{
  return pixelCount;
}

unsigned CConn::getPosition()
{
  return getInStream()->pos();
}

int CConn::securityType()
{
  return csecurity ? csecurity->getType() : -1;
}

ModifiablePixelBuffer *CConn::framebuffer()
{
  return getFramebuffer();
}

void CConn::sendClipboardContent(const char* data)
{
  CConnection::sendClipboardData(data);
}

void CConn::setProcessState(int state)
{
  setState((CConnection::stateEnum)state);
}

void CConn::resetConnection()
{
  initialiseProtocol();
}

void CConn::startProcessing()
{
  static bool recursing = false;

  // I don't think processMsg() is recursion safe, so add this check
  assert(!recursing);

  recursing = true;
  socketReadNotifier->setEnabled(false);
  socketWriteNotifier->setEnabled(false);

  try {
    getOutStream()->cork(true);

    while (processMsg()) {
      qApp->processEvents();
      if (should_disconnect())
        break;
    }

    getOutStream()->cork(false);
  } catch (rdr::EndOfStream& e) {
    recursing = false;
    vlog.info("%s", e.str());
    if (!desktop) {
      vlog.error(_("The connection was dropped by the server before "
                   "the session could be established."));
      abort_connection(_("The connection was dropped by the server "
                         "before the session could be established."));
    } else {
      qApp->quit();
    }
  } catch (rdr::Exception& e) {
    recursing = false;
    abort_connection_with_unexpected_error(e);
  }

  socketReadNotifier->setEnabled(true);
  socketWriteNotifier->setEnabled(socket->outStream().hasBufferedData());

  recursing = false;
}

void CConn::flushSocket()
{
  socket->outStream().flush();

  socketWriteNotifier->setEnabled(socket->outStream().hasBufferedData());
}

////////////////////// CConnection callback methods //////////////////////

// initDone() is called when the serverInit message has been received.  At
// this point we create the desktop window and display it.  We also tell the
// server the pixel format and encodings to use and request the first update.
void CConn::initDone()
{
  // If using AutoSelect with old servers, start in FullColor
  // mode. See comment in autoSelectFormatAndEncoding.
  if (server.beforeVersion(3, 8) && ::autoSelect)
    ::fullColour.setParam(true);

  *serverPF = server.pf();

  setFramebuffer(new PlatformPixelBuffer(server.width(), server.height()));

  desktop = new DesktopWindow(server.width(), server.height(),
                              server.name(), this);
  *fullColourPF = getFramebuffer()->getPF();

  // Force a switch to the format and encoding we'd like
  updatePixelFormat();
  int encNum = encodingNum(::preferredEncoding);
  if (encNum != -1)
    setPreferredEncoding(encNum);
}

// setDesktopSize() is called when the desktop size changes (including when
// it is set initially).
void CConn::setDesktopSize(int w, int h)
{
  CConnection::setDesktopSize(w,h);
  resizeFramebuffer();
}

// setExtendedDesktopSize() is a more advanced version of setDesktopSize()
void CConn::setExtendedDesktopSize(unsigned reason, unsigned result,
                                   int w, int h, const rfb::ScreenSet& layout)
{
  CConnection::setExtendedDesktopSize(reason, result, w, h, layout);

  if ((reason == reasonClient) && (result != resultSuccess)) {
    vlog.error(_("SetDesktopSize failed: %d"), result);
    return;
  }

  resizeFramebuffer();
}

// setName() is called when the desktop name changes
void CConn::setName(const char* name)
{
  CConnection::setName(name);
  desktop->setName(name);
}

// framebufferUpdateStart() is called at the beginning of an update.
// Here we try to send out a new framebuffer update request so that the
// next update can be sent out in parallel with us decoding the current
// one.
void CConn::framebufferUpdateStart()
{
  CConnection::framebufferUpdateStart();

  // For bandwidth estimate
  gettimeofday(&updateStartTime, nullptr);
  updateStartPos = getInStream()->pos();

  // Update the screen prematurely for very slow updates
  updateTimer.start(1000);
}

// framebufferUpdateEnd() is called at the end of an update.
// For each rectangle, the FdInStream will have timed the speed
// of the connection, allowing us to select format and encoding
// appropriately, and then request another incremental update.
void CConn::framebufferUpdateEnd()
{
  unsigned long long elapsed, bps, weight;
  struct timeval now;

  CConnection::framebufferUpdateEnd();

  updateCount++;

  // Calculate bandwidth everything managed to maintain during this update
  gettimeofday(&now, nullptr);
  elapsed = (now.tv_sec - updateStartTime.tv_sec) * 1000000;
  elapsed += now.tv_usec - updateStartTime.tv_usec;
  if (elapsed == 0)
    elapsed = 1;
  bps = (unsigned long long)(getInStream()->pos() -
                             updateStartPos) * 8 *
                            1000000 / elapsed;
  // Allow this update to influence things more the longer it took, to a
  // maximum of 20% of the new value.
  weight = elapsed * 1000 / bpsEstimateWindow;
  if (weight > 200000)
    weight = 200000;
  bpsEstimate = ((bpsEstimate * (1000000 - weight)) +
                 (bps * weight)) / 1000000;
  
  updateTimer.stop();
  desktop->updateWindow();

  // Compute new settings based on updated bandwidth values
  if (::autoSelect)
    autoSelectFormatAndEncoding();
}

// The rest of the callbacks are fairly self-explanatory...

void CConn::setColourMapEntries(int firstColour, int nColours, uint16_t *rgbs)
{
  Q_UNUSED(firstColour)
  Q_UNUSED(nColours)
  Q_UNUSED(rgbs)
  vlog.error(_("Invalid SetColourMapEntries from server!"));
}

void CConn::bell()
{
  // FIXME: QApplication::beep()?
#if defined(__APPLE__)
  cocoa_beep();
#elif defined(WIN32)
  MessageBeep(0xFFFFFFFF); // cf. fltk/src/drivers/WinAPI/Fl_WinAPI_Screen_Driver.cxx:245
#else
  Display* dpy;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  XBell(dpy, 0 /* volume */);
#endif
}

bool CConn::dataRect(const Rect& r, int encoding)
{
  bool ret;

  if (encoding != encodingCopyRect)
    lastServerEncoding = encoding;

  ret = CConnection::dataRect(r, encoding);

  if (ret)
    pixelCount += r.area();

  return ret;
}

void CConn::setCursor(int width, int height, const Point &hotspot,
                      const uint8_t *data)
{
  desktop->setCursor(width, height, hotspot, data);
}

void CConn::setCursorPos(const Point &pos)
{
  desktop->setCursorPos(pos);
}

void CConn::fence(uint32_t flags, unsigned len, const uint8_t data[])
{
  CMsgHandler::fence(flags, len, data);

  if (flags & fenceFlagRequest) {
    // We handle everything synchronously so we trivially honor these modes
    flags = flags & (fenceFlagBlockBefore | fenceFlagBlockAfter);

    writer()->writeFence(flags, len, data);
    return;
  }
}

void CConn::setLEDState(unsigned int state)
{
  vlog.debug("Got server LED state: 0x%08x", state);
  CConnection::setLEDState(state);

  desktop->setLEDState(state);
}

void CConn::handleClipboardRequest()
{
  desktop->handleClipboardRequest();
}

void CConn::handleClipboardAnnounce(bool available)
{
  desktop->handleClipboardAnnounce(available);
}

void CConn::handleClipboardData(const char* data)
{
  desktop->handleClipboardData(data);
}

void CConn::getUserPasswd(bool secure, std::string* user,
                          std::string* password)
{
  credential->getUserPasswd(secure, user, password);
}

bool CConn::showMsgBox(rfb::MsgBoxFlags flags, const char *title,
                       const char *text)
{
  return credential->showMsgBox(flags, title, text);
}

////////////////////// Internal methods //////////////////////

void CConn::resizeFramebuffer()
{
  PlatformPixelBuffer *framebuffer = new PlatformPixelBuffer(server.width(), server.height());
  setFramebuffer(framebuffer);

  desktop->resizeFramebuffer(server.width(), server.height());
}

// autoSelectFormatAndEncoding() chooses the format and encoding appropriate
// to the connection speed:
//
//   First we wait for at least one second of bandwidth measurement.
//
//   Above 16Mbps (i.e. LAN), we choose the second highest JPEG quality,
//   which should be perceptually lossless.
//
//   If the bandwidth is below that, we choose a more lossy JPEG quality.
//
//   If the bandwidth drops below 256 Kbps, we switch to palette mode.
//
//   Note: The system here is fairly arbitrary and should be replaced
//         with something more intelligent at the server end.
//
void CConn::autoSelectFormatAndEncoding()
{
  // Always use Tight
  setPreferredEncoding(encodingTight);

  // Select appropriate quality level
  if (!::noJpeg) {
    int newQualityLevel;
    if (bpsEstimate > 16000000)
      newQualityLevel = 8;
    else
      newQualityLevel = 6;

    if (newQualityLevel != ::qualityLevel) {
      vlog.info(_("Throughput %d kbit/s - changing to quality %d"),
                (int)(bpsEstimate/1000), newQualityLevel);
      ::qualityLevel.setParam(newQualityLevel);
      setQualityLevel(newQualityLevel);
    }
  }

  if (server.beforeVersion(3, 8)) {
    // Xvnc from TightVNC 1.2.9 sends out FramebufferUpdates with
    // cursors "asynchronously". If this happens in the middle of a
    // pixel format change, the server will encode the cursor with
    // the old format, but the client will try to decode it
    // according to the new format. This will lead to a
    // crash. Therefore, we do not allow automatic format change for
    // old servers.
    return;
  }
  
  // Select best color level
  bool newFullColour = (bpsEstimate > 256000);
  if (newFullColour != ::fullColour) {
    if (newFullColour)
      vlog.info(_("Throughput %d kbit/s - full color is now enabled"),
                (int)(bpsEstimate/1000));
    else
      vlog.info(_("Throughput %d kbit/s - full color is now disabled"),
                (int)(bpsEstimate/1000));
    ::fullColour.setParam(newFullColour);
    updatePixelFormat();
  } 
}

// requestNewUpdate() requests an update from the server, having set the
// format and encoding appropriately.
void CConn::updatePixelFormat()
{
  PixelFormat pf;

  if (::fullColour) {
    pf = *fullColourPF;
  }
  else {
    if (::lowColourLevel == 0) {
      pf = verylowColourPF;
    }
    else if (::lowColourLevel == 1) {
      pf = lowColourPF;
    }
    else {
      pf = mediumColourPF;
    }
  }
  char str[256];
  pf.print(str, 256);
  vlog.info(_("Using pixel format %s"),str);
  setPF(pf);
}

void CConn::handleOptions(void *data)
{
  CConn *cc = (CConn*)data;

  // Checking all the details of the current set of encodings is just
  // a pain. Assume something has changed, as resending the encoding
  // list is cheap. Avoid overriding what the auto logic has selected
  // though.
  if (!::autoSelect) {
    int encNum = encodingNum(::preferredEncoding);

    if (encNum != -1)
      cc->setPreferredEncoding(encNum);
  }

  if (::customCompressLevel)
    cc->setCompressLevel(::compressLevel);
  else
    cc->setCompressLevel(-1);

  if (!::noJpeg && !::autoSelect)
    cc->setQualityLevel(::qualityLevel);
  else
    cc->setQualityLevel(-1);

  cc->updatePixelFormat();
}

void CConn::handleUpdateTimeout(rfb::Timer*)
{
  try {
    framebufferUpdateEnd();
  } catch (rdr::Exception& e) {
    abort_connection("%s", e.str());
  }
}
