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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QGuiApplication>
#include <QTimer>
#include <QCursor>
#include <QClipboard>
#include <QPixmap>
#include <time.h>
#include "rfb/LogWriter.h"
#include "rfb/fenceTypes.h"
#include "rfb/CMsgWriter.h"
#include "network/TcpSocket.h"
#include "parameters.h"
#include "PlatformPixelBuffer.h"
#include "i18n.h"
#include "appmanager.h"
#include "vnccredential.h"
#include "vncconnection.h"
#include "CConn.h"
#undef asprintf

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

CConn::CConn(QVNCConnection *cfacade)
 : CConnection()
 , serverHost("")
 , serverPort(5900)
 , facade(cfacade)
 , cursor(nullptr)
 , updateCount(0)
 , pixelCount(0)
 , serverPF(new PixelFormat)
 , fullColourPF(new PixelFormat(32, 24, false, true, 255, 255, 255, 16, 8, 0))
 , lastServerEncoding((unsigned int)-1)
 , updateStartPos(0)
 , bpsEstimate(20000000)
{
  setShared(ViewerConfig::config()->shared());
  
  supportsLocalCursor = true;
  supportsCursorPosition = true;
  supportsDesktopResize = true;
  supportsLEDState = true;
  
  initialiseProtocol();
  if (!CSecurity::upg) {
    CSecurity::upg = new VNCCredential;
  }
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
  if (!CSecurity::msg) {
    CSecurity::msg = new VNCCredential;
  }
#endif
  if (ViewerConfig::config()->customCompressLevel()) {
    setCompressLevel(ViewerConfig::config()->compressLevel());
  }

  if (!ViewerConfig::config()->noJpeg()) {
    setQualityLevel(ViewerConfig::config()->qualityLevel());
  }
}

CConn::~CConn()
{
  resetConnection();
  delete serverPF;
  delete fullColourPF;
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

void CConn::sendClipboardContent()
{
  QString text = QGuiApplication::clipboard()->text();
  CConnection::sendClipboardData(text.toStdString().c_str());
}

void CConn::setProcessState(int state)
{
  setState((CConnection::stateEnum)state);
}

void CConn::resetConnection()
{
  delete cursor;
  cursor = nullptr;
  initialiseProtocol();
}

////////////////////// CConnection callback methods //////////////////////

// initDone() is called when the serverInit message has been received.  At
// this point we create the desktop window and display it.  We also tell the
// server the pixel format and encodings to use and request the first update.
void CConn::initDone()
{
  // If using AutoSelect with old servers, start in FullColor
  // mode. See comment in autoSelectFormatAndEncoding.
  if (server.beforeVersion(3, 8) && ViewerConfig::config()->autoSelect())
    ViewerConfig::config()->setFullColour(true);

  *serverPF = server.pf();

  setFramebuffer(new PlatformPixelBuffer(server.width(), server.height()));
  emit facade->newVncWindowRequested(server.width(), server.height(), server.name() /*, serverPF_, this */);
  *fullColourPF = getFramebuffer()->getPF();

  // Force a switch to the format and encoding we'd like
  updatePixelFormat();
  int encNum = encodingNum(ViewerConfig::config()->preferredEncoding().toStdString().c_str());
  if (encNum != -1)
    setPreferredEncoding(encNum);
}

// setName() is called when the desktop name changes
void CConn::setName(const char* name)
{
  CConnection::setName(name);
  AppManager::instance()->setWindowName(name);
}

// framebufferUpdateStart() is called at the beginning of an update.
// Here we try to send out a new framebuffer update request so that the
// next update can be sent out in parallel with us decoding the current
// one.
void CConn::framebufferUpdateStart()
{
  CConnection::framebufferUpdateStart();

  // For bandwidth estimate
  gettimeofday(&updateStartTime, NULL);
  updateStartPos = getInStream()->pos();

  // Update the screen prematurely for very slow updates
  facade->updateTimer()->setInterval(1000);
  facade->updateTimer()->start();
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
  gettimeofday(&now, NULL);
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

  facade->updateTimer()->stop();
  emit facade->refreshFramebufferEnded();

  // Compute new settings based on updated bandwidth values
  if (ViewerConfig::config()->autoSelect())
    autoSelectFormatAndEncoding();
}

// The rest of the callbacks are fairly self-explanatory...

void CConn::setColourMapEntries(int firstColour, int nColours, uint16_t *rgbs)
{
  Q_UNUSED(firstColour)
  Q_UNUSED(nColours)
  Q_UNUSED(rgbs)
  vlog.error("Invalid SetColourMapEntries from server!");
}

void CConn::bell()
{
  emit facade->bellRequested();
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
  bool emptyCursor = true;
  for (int i = 0; i < width * height; i++) {
    if (data[i*4 + 3] != 0) {
      emptyCursor = false;
      break;
    }
  }
  if (emptyCursor) {
    if (ViewerConfig::config()->dotWhenNoCursor()) {
      static const char * dotcursor_xpm[] = {
        "5 5 2 1",
        ".	c #000000",
        " 	c #FFFFFF",
        "     ",
        " ... ",
        " ... ",
        " ... ",
        "     "};
      delete cursor;
      cursor = new QCursor(QPixmap(dotcursor_xpm), 2, 2);
    }
    else {
      static const char * emptycursor_xpm[] = {
        "2 2 1 1",
        ".	c None",
        "..",
        ".."};
      delete cursor;
      cursor = new QCursor(QPixmap(emptycursor_xpm), 0, 0);
    }
  }
  else {
    //qDebug() << "QVNCConnection::setCursor: w=" << width << ", h=" << height << ", data=" << data;
    QImage image(data, width, height, QImage::Format_RGBA8888);
    delete cursor;
    cursor = new QCursor(QPixmap::fromImage(image), hotspot.x, hotspot.y);
  }
  emit facade->cursorChanged(*cursor);
}

void CConn::setCursorPos(const Point &pos)
{
  emit facade->cursorPositionChanged(pos.x, pos.y);
}

void CConn::fence(uint32_t flags, unsigned len, const char data[])
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
//  qDebug() << "QVNCConnection::setLEDState";
  vlog.debug("Got server LED state: 0x%08x", state);
  CConnection::setLEDState(state);

  emit facade->ledStateChanged(state);
}

void CConn::handleClipboardRequest()
{
  sendClipboardContent();
}

void CConn::handleClipboardAnnounce(bool available)
{
  emit facade->clipboardAnnounced(available);
  requestClipboard();
}

void CConn::handleClipboardData(const char* data)
{
  emit facade->clipboardDataReceived(data);
}


////////////////////// Internal methods //////////////////////

void CConn::resizeFramebuffer()
{
  //qDebug() << "QVNCConnection::resizeFramebuffer(): width=" << server.width() << ",height=" << server.height();
  PlatformPixelBuffer *framebuffer = new PlatformPixelBuffer(server.width(), server.height());
  setFramebuffer(framebuffer);

  emit facade->framebufferResized(server.width(), server.height());
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
  //qDebug() << "QVNCConnection::autoSelectFormatAndEncoding";

  // Always use Tight
  setPreferredEncoding(encodingTight);

  // Select appropriate quality level
  if (!ViewerConfig::config()->noJpeg()) {
    int newQualityLevel;
    if (bpsEstimate > 16000000)
      newQualityLevel = 8;
    else
      newQualityLevel = 6;

    if (newQualityLevel != ViewerConfig::config()->qualityLevel()) {
      vlog.info(_("Throughput %d kbit/s - changing to quality %d"),
                (int)(bpsEstimate/1000), newQualityLevel);
      ViewerConfig::config()->setQualityLevel(newQualityLevel);
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
  if (newFullColour != ViewerConfig::config()->fullColour()) {
    if (newFullColour)
      vlog.info(_("Throughput %d kbit/s - full color is now enabled"),
                (int)(bpsEstimate/1000));
    else
      vlog.info(_("Throughput %d kbit/s - full color is now disabled"),
                (int)(bpsEstimate/1000));
    ViewerConfig::config()->setFullColour(newFullColour);
    updatePixelFormat();
  } 
}

// requestNewUpdate() requests an update from the server, having set the
// format and encoding appropriately.
void CConn::updatePixelFormat()
{
  PixelFormat pf;

  if (ViewerConfig::config()->fullColour()) {
    pf = *fullColourPF;
  }
  else {
    if (ViewerConfig::config()->lowColourLevel() == 0) {
      pf = verylowColourPF;
    }
    else if (ViewerConfig::config()->lowColourLevel() == 1) {
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
