/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2011 D. R. Commander.  All Rights Reserved.
 * Copyright 2009-2026 Pierre Ossman for Cendio AB
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
#include <config.h>
#endif

#include <assert.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef HAVE_GNUTLS
#include <gnutls/x509.h>
#endif

#include <QMessageBox>

#include <core/Exception.h>
#include <core/LogWriter.h>
#include <core/Timer.h>
#include <core/i18n.h>
#include <core/string.h>
#include <core/time.h>
#include <core/xdgdirs.h>

#include <rdr/FdInStream.h>
#include <rdr/FdOutStream.h>

#include <rfb/CMsgWriter.h>
#include <rfb/CSecurity.h>
#include <rfb/Exception.h>
#include <rfb/Security.h>
#include <rfb/fenceTypes.h>
#include <rfb/obfuscate.h>
#include <rfb/screenTypes.h>

#include <network/TcpSocket.h>
#ifndef WIN32
#include <network/UnixSocket.h>
#endif

#include <FL/Fl.H>
#include <FL/fl_ask.H>

#include "AudioOutput.h"
#include "AuthDialog.h"
#include "CConn.h"
#include "OptionsDialog.h"
#include "DesktopWindow.h"
#include "mainloop.h"
#include "parameters.h"

std::string CConn::savedUsername;
std::string CConn::savedPassword;

#ifdef WIN32
#include "win32.h"
#endif

static core::LogWriter vlog("CConn");

// 8 colours (1 bit per component)
static const rfb::PixelFormat verylowColourPF(8, 3,false, true,
                                              1, 1, 1, 2, 1, 0);
// 64 colours (2 bits per component)
static const rfb::PixelFormat lowColourPF(8, 6, false, true,
                                          3, 3, 3, 4, 2, 0);
// 256 colours (2-3 bits per component)
static const rfb::PixelFormat mediumColourPF(8, 8, false, true,
                                             7, 7, 3, 5, 2, 0);

// Time new bandwidth estimates are weighted against (in ms)
static const unsigned bpsEstimateWindow = 1000;

CConn::CConn()
  : serverPort(0), sock(nullptr),
    msgTimer(this, &CConn::processNextMsg),
    authDialog(nullptr), verifyDialog(nullptr), desktop(nullptr),
    audioOutput(nullptr),
    updateCount(0), pixelCount(0),
    lastServerEncoding((unsigned int)-1), bpsEstimate(20000000),
    updateTimer(this, &CConn::handleUpdateTimeout)
{
  setShared(::shared);

  supportsLocalCursor = true;
  supportsCursorPosition = true;
  supportsDesktopResize = true;
  supportsLEDState = true;

#ifdef HAVE_AUDIO
  // Only ask the server to send audio if there is a device that can
  // play it. Opening that device is deferred until the server actually
  // has something for us.
  if (playAudio) {
    audioOutput = AudioOutput::create();
    supportsAudio = audioOutput != nullptr;
  }
#endif

  if (customCompressLevel)
    setCompressLevel(::compressLevel);

  setQualityLevel(::qualityLevel);

  OptionsDialog::addCallback(handleOptions, this);
}

CConn::~CConn()
{
  close();

  OptionsDialog::removeCallback(handleOptions);

  delete authDialog;
  delete verifyDialog;

  if (desktop)
    delete desktop;

  delete audioOutput;

  if (sock) {
    struct timeval now;

    sock->shutdownWrite();

    // Do a graceful close by waiting for the peer (up to 250 ms)
    // FIXME: should do this asynchronously
    gettimeofday(&now, nullptr);
    while (core::msSince(&now) < 250) {
      bool done;

      done = false;
      while (true) {
        try {
          sock->inStream().skip(sock->inStream().avail());
          if (!sock->inStream().hasData(1))
            break;
        } catch (std::exception&) {
          done = true;
          break;
        }
      }

      if (done)
        break;

  #ifdef WIN32
      Sleep(10);
  #else
      usleep(10000);
  #endif
    }

    Fl::remove_fd(sock->getFd());

    delete sock;
  }
}

void CConn::connect(const char* vncServerName, network::Socket* socket)
{
  sock = socket;
  if(sock == nullptr) {
    try {
#ifndef WIN32
      if (strchr(vncServerName, '/') != nullptr) {
        sock = new network::UnixSocket(vncServerName);
        serverHost = sock->getPeerAddress();
        vlog.info(_("Connected to socket %s"), serverHost.c_str());
      } else
#endif
      {
        network::getHostAndPort(vncServerName, &serverHost, &serverPort);

        sock = new network::TcpSocket(serverHost.c_str(), serverPort);
        vlog.info(_("Connected to host %s port %d"),
                  serverHost.c_str(), serverPort);
      }
    } catch (std::exception& e) {
      vlog.error("%s", e.what());
      abort_connection(_("Failed to connect to \"%s\":\n\n%s"),
                       vncServerName, e.what());
      return;
    }
  }

  Fl::add_fd(sock->getFd(), FL_READ | FL_EXCEPT,
             [](FL_SOCKET, void* data) {
               ((CConn*)data)->socketEvent();
             }, this);

  setServerName(serverHost.c_str());
  setStreams(&sock->inStream(), &sock->outStream());

  initialiseProtocol();
}

std::string CConn::connectionInfo()
{
  std::string infoText;

  char pfStr[100];

  infoText += core::format(_("Desktop name: %.80s"), server.name());
  infoText += "\n";

  infoText += core::format(_("Host: %.80s port: %d"),
                           serverHost.c_str(), serverPort);
  infoText += "\n";

  infoText += core::format(_("Size: %d x %d"),
                           server.width(), server.height());
  infoText += "\n";

  // TRANSLATORS: Will be filled in with a string describing the
  // protocol pixel format in a fairly language neutral way
  server.pf().print(pfStr, 100);
  infoText += core::format(_("Pixel format: %s"), pfStr);
  infoText += "\n";

  infoText += core::format(_("Requested encoding: %s"),
                           rfb::encodingName(getPreferredEncoding()));
  infoText += "\n";

  infoText += core::format(_("Last used encoding: %s"),
                           rfb::encodingName(lastServerEncoding));
  infoText += "\n";

  infoText += core::format(_("Line speed estimate: %d kbit/s"),
                           (int)(bpsEstimate / 1000));
  infoText += "\n";

  infoText += core::format(_("Protocol version: %d.%d"),
                           server.majorVersion, server.minorVersion);
  infoText += "\n";

  infoText += core::format(_("Security method: %s"),
                           rfb::secTypeName(csecurity->getType()));
  infoText += "\n";

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
  return sock->inStream().pos();
}

void CConn::socketEvent()
{
  // Stop monitoring the socket for now and start processing incoming
  // data asynchronously
  Fl::remove_fd(sock->getFd());
  msgTimer.start(0);

  // Coalesce data until we're fully done processing things
  getOutStream()->cork(true);
}

void CConn::processNextMsg(core::Timer*)
{
  static bool recursing = false;
  bool again;
  int when;

  // I don't think processMsg() is recursion safe, so add this check
  assert(!recursing);

  recursing = true;

  again = false;
  try {
    // We might have been called to flush unwritten socket data
    sock->outStream().flush();

    again = processMsg();
  } catch (rdr::end_of_stream& e) {
    if (authDialog)
      authDialog->hide();
    if (verifyDialog)
      verifyDialog->hide();
    vlog.info("%s", e.what());
    if (!desktop) {
      vlog.error(_("The connection was dropped by the server before "
                   "the session could be established."));
      abort_connection(_("The connection was dropped by the server "
                       "before the session could be established."));
    } else {
      disconnect();
    }
  } catch (rfb::auth_error& e) {
    if (authDialog)
      authDialog->hide();
    if (verifyDialog)
      verifyDialog->hide();
    savedUsername.clear();
    savedPassword.clear();
    vlog.error(_("Authentication failed: %s"), e.what());
    abort_connection(_("Failed to authenticate with the server. Reason "
                       "given by the server:\n\n%s"), e.what());
  } catch (std::exception& e) {
    if (authDialog)
      authDialog->hide();
    if (verifyDialog)
      verifyDialog->hide();
    vlog.error("%s", e.what());
    abort_connection_unexpected(e);
  }

  recursing = false;

  if (again) {
    msgTimer.repeat();
    return;
  }

  // Out of data, go back to waiting

  getOutStream()->cork(false);

  when = FL_READ | FL_EXCEPT;
  if (sock->outStream().hasBufferedData())
    when |= FL_WRITE;

  Fl::add_fd(sock->getFd(), when,
             [](FL_SOCKET, void* data) {
               ((CConn*)data)->socketEvent();
             }, this);
}

////////////////////// CConnection callback methods //////////////////////

void CConn::credentialsRequested(bool secure, bool needsUser,
                                 bool needsPassword)
{
  const char *passwordFileName(passwordFile);

  assert(needsPassword);
  char *envUsername = getenv("VNC_USERNAME");
  char *envPassword = getenv("VNC_PASSWORD");

  if (needsUser && envUsername && envPassword) {
    setCredentials(envUsername, envPassword);
    resumeProcessing();
    return;
  }

  if (!needsUser && envPassword) {
    setCredentials("", envPassword);
    resumeProcessing();
    return;
  }

  if (needsUser && !savedUsername.empty() && !savedPassword.empty()) {
    setCredentials(savedUsername, savedPassword);
    resumeProcessing();
    return;
  }

  if (!needsUser && !savedPassword.empty()) {
    setCredentials("", savedPassword);
    resumeProcessing();
    return;
  }

  if (!needsUser && passwordFileName[0]) {
    std::vector<uint8_t> obfPwd(8);
    FILE* fp;

    fp = fopen(passwordFileName, "rb");
    if (!fp) {
      abort_connection_unexpected("%s: %s",
                                  core::format(_("Failed to open \"%s\""),
                                               passwordFileName).c_str(),
                                  strerror(errno));
      return;
    }

    obfPwd.resize(fread(obfPwd.data(), 1, obfPwd.size(), fp));
    fclose(fp);

    setCredentials("", rfb::deobfuscate(obfPwd.data(), obfPwd.size()));
    resumeProcessing();
    return;
  }

  assert(authDialog == nullptr);
  authDialog = new AuthDialog(secure, needsUser, needsPassword);
  QObject::connect(authDialog, &QDialog::accepted,
                   [this]() { this->handleAuthOK(); });
  QObject::connect(authDialog, &QDialog::rejected,
                   [this]() { this->handleAuthCancel(); });

  authDialog->show();
}

// initDone() is called when the serverInit message has been received.  At
// this point we create the desktop window and display it.  We also tell the
// server the pixel format and encodings to use and request the first update.
void CConn::initDone()
{
  // If using AutoSelect with old servers, start in FullColor
  // mode. See comment in autoSelectFormatAndEncoding. 
  if (server.beforeVersion(3, 8) && autoSelect)
    fullColour.setParam(true);

  desktop = new DesktopWindow(server.width(), server.height(), this);
  fullColourPF = desktop->getPreferredPF();

  // Force a switch to the format and encoding we'd like
  updateEncoding();
  updatePixelFormat();
}

void CConn::certificateReceived(unsigned int status,
                                const uint8_t* certificate,
                                size_t length)
{
#ifndef HAVE_GNUTLS
  (void)status;
  (void)certificate;
  (void)length;
  abort_connection_unexpected("TLS support not enabled");
#else
  const unsigned allowed_errors =
    GNUTLS_CERT_INVALID |
    GNUTLS_CERT_SIGNER_NOT_FOUND |
    GNUTLS_CERT_SIGNER_NOT_CA |
    GNUTLS_CERT_NOT_ACTIVATED |
    GNUTLS_CERT_EXPIRED |
    GNUTLS_CERT_INSECURE_ALGORITHM |
    GNUTLS_CERT_UNEXPECTED_OWNER;
  gnutls_datum_t status_str;
  unsigned int fatal_status;

  gnutls_datum_t cert_datum;
  gnutls_x509_crt_t crt;
  int err, known;

  const char *hostsDir;

  gnutls_datum_t info_datum;
  size_t len;

  assert(status != 0);

  fatal_status = status & (~allowed_errors);

  if (fatal_status != 0) {
    err = gnutls_certificate_verification_status_print(fatal_status,
                                                       GNUTLS_CRT_X509,
                                                       &status_str,
                                                       0);
    if (err != GNUTLS_E_SUCCESS) {
      abort_connection_unexpected(_("Failed to get certificate problem "
                                    "description: %s"),
                                  gnutls_strerror(err));
      return;
    }

    abort_connection(_("Invalid server certificate: %s"),
                     status_str.data);

    gnutls_free(status_str.data);

    return;
  }

  err = gnutls_certificate_verification_status_print(status,
                                                     GNUTLS_CRT_X509,
                                                     &status_str,
                                                     0);
  if (err != GNUTLS_E_SUCCESS) {
    abort_connection_unexpected(_("Failed to get certificate problem "
                                  "description: %s"),
                                gnutls_strerror(err));
    return;
  }

  vlog.info(_("Server certificate problems: %s"), status_str.data);

  gnutls_free(status_str.data);

  /* Certificate has some user overridable problems, so TOFU time */

  hostsDir = core::getvncstatedir();
  if (hostsDir == nullptr) {
    abort_connection_unexpected(_("Could not determine VNC state "
                                  "directory path"));
    return;
  }

  std::string dbPath;
  dbPath = (std::string)hostsDir + "/x509_known_hosts";

  cert_datum.data = (uint8_t*)certificate;
  cert_datum.size = length;

  known = gnutls_verify_stored_pubkey(dbPath.c_str(), nullptr,
                                      getServerName(), nullptr,
                                      GNUTLS_CRT_X509, &cert_datum, 0);

  /* Previously known? */
  if (known == GNUTLS_E_SUCCESS) {
    vlog.info(_("Server has an existing security exception"));
    approveCertificate();
    resumeProcessing();
    return;
  }

  if ((known != GNUTLS_E_NO_CERTIFICATE_FOUND) &&
      (known != GNUTLS_E_CERTIFICATE_KEY_MISMATCH)) {
    abort_connection_unexpected(_("Failed to load list of servers with "
                                  "a security exception: %s"),
                                gnutls_strerror(known));
    return;
  }

  if (known == GNUTLS_E_NO_CERTIFICATE_FOUND)
    vlog.info(_("Server host is not previously known"));
  else
    vlog.info(_("Server host certificate has changed"));

  gnutls_x509_crt_init(&crt);
  err = gnutls_x509_crt_import(crt, &cert_datum, GNUTLS_X509_FMT_DER);
  if (err != GNUTLS_E_SUCCESS) {
    abort_connection_unexpected(_("Failed to decode server "
                                  "certificate: %s"),
                                gnutls_strerror(err));
    return;
  }

  err = gnutls_x509_crt_print(crt, GNUTLS_CRT_PRINT_ONELINE,
                              &info_datum);
  gnutls_x509_crt_deinit(crt);
  if (err != GNUTLS_E_SUCCESS) {
    abort_connection_unexpected(_("Failed to format server "
                                  "certificate for display: %s"),
                                gnutls_strerror(err));
    return;
  }

  len = strlen((char*)info_datum.data);
  for (size_t i = 0; i < len - 1; i++) {
    if (info_datum.data[i] == ',' && info_datum.data[i + 1] == ' ')
      info_datum.data[i] = '\n';
  }

  vlog.info("%s", info_datum.data);

  gnutls_free(info_datum.data);

  pendingCertificateStatus = status;
  pendingCertificateNew = (known == GNUTLS_E_NO_CERTIFICATE_FOUND);
  pendingCertificate.resize(length);
  memcpy(pendingCertificate.data(), certificate, length);

  assert(verifyDialog == nullptr);
  handleCertificateOK();
#endif
}

void CConn::hostKeyReceived(const uint8_t* key, size_t length,
                            const char* fingerprint)
{
  std::string text;

  // FIXME: Should save this for TOFU
  (void)key;
  (void)length;

  text = core::format(
    _("The server has provided the following identifying information:\n"
      "\n"
      "Fingerprint: %s\n"
      "\n"
      "Do you want to continue connecting to this server?"),
    fingerprint);

  assert(verifyDialog == nullptr);
  verifyDialog = new QMessageBox;
  verifyDialog->setIcon(QMessageBox::Warning);
  verifyDialog->setWindowTitle(_("Verify server key"));
  verifyDialog->setText(text.c_str());
  verifyDialog->addButton(_("Continue"), QMessageBox::AcceptRole);
  verifyDialog->addButton(QMessageBox::Cancel);
  verifyDialog->setDefaultButton(QMessageBox::Cancel);

  QObject::connect(verifyDialog, &QDialog::accepted,
                   [this]() { this->handleHostKeyOK(); });
  QObject::connect(verifyDialog, &QDialog::rejected,
                   [this]() { this->handleHostKeyCancel(); });

  verifyDialog->show();
}

void CConn::setExtendedDesktopSize(unsigned reason, unsigned result,
                                   int w, int h,
                                   const rfb::ScreenSet& layout)
{
  CConnection::setExtendedDesktopSize(reason, result, w, h, layout);

  if (reason == rfb::reasonClient)
    desktop->setDesktopSizeDone(result);
}

// setName() is called when the desktop name changes
void CConn::setName(const char* name)
{
  CConnection::setName(name);
  desktop->updateCaption();
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
  updateStartPos = sock->inStream().pos();

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
  bps = (unsigned long long)(sock->inStream().pos() -
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
  if (autoSelect) {
    updateEncoding();
    updateQualityLevel();
    updatePixelFormat();
  }
}

// The rest of the callbacks are fairly self-explanatory...

void CConn::bell()
{
  fl_beep();
}

bool CConn::dataRect(const core::Rect& r, int encoding)
{
  bool ret;

  if (encoding != rfb::encodingCopyRect)
    lastServerEncoding = encoding;

  ret = CConnection::dataRect(r, encoding);

  if (ret)
    pixelCount += r.area();

  return ret;
}

void CConn::setCursor(int width, int height, const core::Point& hotspot,
                      const uint8_t* data)
{
  CConnection::setCursor(width, height, hotspot, data);

  desktop->setCursor();
}

void CConn::setCursorPos(const core::Point& pos)
{
  desktop->setCursorPos(pos);
}

void CConn::setLEDState(unsigned int state)
{
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

void CConn::handleAudioBegin()
{
  if (audioOutput == nullptr)
    return;

  audioOutput->start();
}

void CConn::handleAudioEnd()
{
  if (audioOutput == nullptr)
    return;

  audioOutput->stop();
}

void CConn::handleAudioData(const uint8_t* data, size_t length)
{
  if (audioOutput == nullptr)
    return;

  audioOutput->play(data, length);
}


////////////////////// Internal methods //////////////////////

void CConn::resizeFramebuffer()
{
  desktop->resizeFramebuffer(server.width(), server.height());
}

void CConn::updateEncoding()
{
  int encNum;

  if (autoSelect)
    encNum = rfb::encodingTight;
  else
    encNum = rfb::encodingNum(::preferredEncoding.getValueStr().c_str());

  if (encNum != -1)
    setPreferredEncoding(encNum);
}

void CConn::updateCompressLevel()
{
  if (customCompressLevel)
    setCompressLevel(::compressLevel);
  else
    setCompressLevel(-1);
}

void CConn::updateQualityLevel()
{
  int newQualityLevel;

  if (!autoSelect)
    newQualityLevel = ::qualityLevel;
  else {
    // Above 16Mbps (i.e. LAN), we choose the second highest JPEG
    // quality, which should be perceptually lossless. If the bandwidth
    // is below that, we choose a more lossy JPEG quality.

    if (bpsEstimate > 16000000)
      newQualityLevel = 8;
    else
      newQualityLevel = 6;

    if (newQualityLevel != getQualityLevel()) {
      vlog.info(_("Throughput %d kbit/s - changing to quality %d"),
                (int)(bpsEstimate/1000), newQualityLevel);
    }
  }

  setQualityLevel(newQualityLevel);
}

void CConn::updatePixelFormat()
{
  bool useFullColour;
  rfb::PixelFormat pf;

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

  useFullColour = fullColour;

  // If the bandwidth drops below 256 Kbps, we switch to palette mode.
  if (autoSelect) {
    useFullColour = (bpsEstimate > 256000);
    if (useFullColour != (server.pf() == fullColourPF)) {
      if (useFullColour)
        vlog.info(_("Throughput %d kbit/s - full color is now enabled"),
                  (int)(bpsEstimate/1000));
      else
        vlog.info(_("Throughput %d kbit/s - full color is now disabled"),
                  (int)(bpsEstimate/1000));
    }
  }

  if (useFullColour) {
    pf = fullColourPF;
  } else {
    if (lowColourLevel == 0)
      pf = verylowColourPF;
    else if (lowColourLevel == 1)
      pf = lowColourPF;
    else
      pf = mediumColourPF;
  }

  if (pf != server.pf()) {
    char str[256];
    pf.print(str, 256);
    vlog.info(_("Using pixel format %s"),str);
    setPF(pf);
  }
}

void CConn::handleOptions(void *data)
{
  CConn *self = (CConn*)data;

  self->updateEncoding();
  self->updateCompressLevel();
  self->updateQualityLevel();
  self->updatePixelFormat();
}

void CConn::handleUpdateTimeout(core::Timer*)
{
  desktop->updateWindow();

  updateTimer.repeat();
}

void CConn::handleAuthOK()
{
  bool keepPasswd;
  std::string user;
  std::string password;

  assert(authDialog);

  if (reconnectOnError)
    keepPasswd = authDialog->getKeepPassword();
  else
    keepPasswd = false;

  if (!authDialog->getUser().empty()) {
    user = authDialog->getUser();
    if (keepPasswd)
      savedUsername = authDialog->getUser();
  }
  password = authDialog->getPassword();
  if (keepPasswd)
    savedPassword = authDialog->getPassword();

  authDialog->deleteLater();
  authDialog = nullptr;

  setCredentials(user, password);
  resumeProcessing();
}

void CConn::handleAuthCancel()
{
  assert(authDialog);

  authDialog->deleteLater();
  authDialog = nullptr;

  vlog.info(_("Authentication cancelled"));
  disconnect();
}

#ifdef HAVE_GNUTLS
void CConn::handleCertificateOK()
{
  gnutls_datum_t cert_datum;
  gnutls_x509_crt_t crt;
  int err;

  gnutls_datum_t info_datum;
  std::string info;
  size_t len;

  std::string title, text;

  cert_datum.data = pendingCertificate.data();
  cert_datum.size = pendingCertificate.size();

  // This will be empty on the first run
  if (verifyDialog != nullptr) {
    verifyDialog->deleteLater();
    verifyDialog = nullptr;

    if (pendingCertificateStatus == 0) {
      const char *hostsDir;

      hostsDir = core::getvncstatedir();
      if (hostsDir == nullptr) {
        abort_connection_unexpected(_("Could not determine VNC state "
                                      "directory path"));
        return;
      }

      std::string dbPath;
      dbPath = (std::string)hostsDir + "/x509_known_hosts";

      if (gnutls_store_pubkey(dbPath.c_str(), nullptr,
                              getServerName(), nullptr,
                              GNUTLS_CRT_X509, &cert_datum, 0, 0))
        vlog.error(_("Failed to store server certificate to list of "
                    "servers with a security exception"));

      vlog.info(_("Security exception added for server host"));

      approveCertificate();
      resumeProcessing();
      return;
    }
  }

  gnutls_x509_crt_init(&crt);
  err = gnutls_x509_crt_import(crt, &cert_datum, GNUTLS_X509_FMT_DER);
  if (err != GNUTLS_E_SUCCESS) {
    abort_connection_unexpected(_("Failed to decode server "
                                  "certificate: %s"),
                                gnutls_strerror(err));
    return;
  }

  err = gnutls_x509_crt_print(crt, GNUTLS_CRT_PRINT_ONELINE,
                              &info_datum);
  gnutls_x509_crt_deinit(crt);
  if (err != GNUTLS_E_SUCCESS) {
    abort_connection_unexpected(_("Failed to format server "
                                  "certificate for display: %s"),
                                gnutls_strerror(err));
    return;
  }

  len = strlen((char*)info_datum.data);
  for (size_t i = 0; i < len - 1; i++) {
    if (info_datum.data[i] == ',' && info_datum.data[i + 1] == ' ')
      info_datum.data[i] = '\n';
  }

  info = (const char*)info_datum.data;

  gnutls_free(info_datum.data);

  /* New host */
  if (pendingCertificateNew) {
    if (pendingCertificateStatus & (GNUTLS_CERT_INVALID |
                                    GNUTLS_CERT_SIGNER_NOT_FOUND |
                                    GNUTLS_CERT_SIGNER_NOT_CA)) {
      title = _("Unknown certificate issuer");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        _("This certificate has been signed by an unknown authority:"),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~(GNUTLS_CERT_INVALID |
                                    GNUTLS_CERT_SIGNER_NOT_FOUND |
                                    GNUTLS_CERT_SIGNER_NOT_CA);
    } else if (pendingCertificateStatus & GNUTLS_CERT_NOT_ACTIVATED) {
      title = _("Certificate is not yet valid");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        _("This certificate is not yet valid:"),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~GNUTLS_CERT_NOT_ACTIVATED;
    } else if (pendingCertificateStatus & GNUTLS_CERT_EXPIRED) {
      title = _("Expired certificate");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        _("This certificate has expired:"),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~GNUTLS_CERT_EXPIRED;
    } else if (pendingCertificateStatus & GNUTLS_CERT_INSECURE_ALGORITHM) {
      title = _("Insecure certificate algorithm");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        _("This certificate uses an insecure algorithm:"),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~GNUTLS_CERT_INSECURE_ALGORITHM;
    } else if (pendingCertificateStatus & GNUTLS_CERT_UNEXPECTED_OWNER) {
      title = _("Certificate hostname mismatch");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        core::format(
          _("The specified hostname \"%s\" does not match the "
            "certificate provided by the server:"),
          getServerName()).c_str(),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~GNUTLS_CERT_UNEXPECTED_OWNER;
    } else if (pendingCertificateStatus != 0) {
      vlog.error(_("Unhandled server certificate problems: 0x%x"),
                 pendingCertificateStatus);
      abort_connection_unexpected(_("Unhandled server certificate "
                                    "problems"));
      return;
    }
  } else  {
    if (pendingCertificateStatus & (GNUTLS_CERT_INVALID |
                                    GNUTLS_CERT_SIGNER_NOT_FOUND |
                                    GNUTLS_CERT_SIGNER_NOT_CA)) {
      title = _("Unexpected server certificate");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        _("This host is previously known with a different certificate, "
          "and the new certificate has been signed by an unknown "
          "authority:"),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~(GNUTLS_CERT_INVALID |
                                    GNUTLS_CERT_SIGNER_NOT_FOUND |
                                    GNUTLS_CERT_SIGNER_NOT_CA);
    } else if (pendingCertificateStatus & GNUTLS_CERT_NOT_ACTIVATED) {
      title = _("Unexpected server certificate");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        _("This host is previously known with a different certificate, "
          "and the new certificate is not yet valid:"),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~GNUTLS_CERT_NOT_ACTIVATED;
    } else if (pendingCertificateStatus & GNUTLS_CERT_EXPIRED) {
      title = _("Unexpected server certificate");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        _("This host is previously known with a different certificate, "
          "and the new certificate has expired:"),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~GNUTLS_CERT_EXPIRED;
    } else if (pendingCertificateStatus & GNUTLS_CERT_INSECURE_ALGORITHM) {
      title = _("Unexpected server certificate");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        _("This host is previously known with a different certificate, "
          "and the new certificate uses an insecure algorithm:"),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~GNUTLS_CERT_INSECURE_ALGORITHM;
    } else if (pendingCertificateStatus & GNUTLS_CERT_UNEXPECTED_OWNER) {
      title = _("Unexpected server certificate");
      text = core::format(
        "%s\n\n%s\n\n%s\n\n%s",
        core::format(
          _("This host is previously known with a different "
            "certificate, and the specified hostname \"%s\" does not "
            "match the new certificate provided by the server:"),
          getServerName()).c_str(),
        info.c_str(),
        _("Someone could be trying to impersonate the site and you "
          "should not continue."),
        _("Do you want to make an exception for this server?"));

      pendingCertificateStatus &= ~GNUTLS_CERT_UNEXPECTED_OWNER;
    } else if (pendingCertificateStatus != 0) {
      vlog.error(_("Unhandled server certificate problems: 0x%x"),
                 pendingCertificateStatus);
      abort_connection_unexpected(_("Unhandled server certificate "
                                    "problems"));
      return;
    }
  }

  assert(verifyDialog == nullptr);
  verifyDialog = new QMessageBox;
  verifyDialog->setIcon(QMessageBox::Warning);
  verifyDialog->setWindowTitle(title.c_str());
  verifyDialog->setText(text.c_str());
  verifyDialog->addButton(_("Add exception"), QMessageBox::AcceptRole);
  verifyDialog->addButton(QMessageBox::Cancel);
  verifyDialog->setDefaultButton(QMessageBox::Cancel);

  QObject::connect(verifyDialog, &QDialog::accepted,
                   [this]() { this->handleCertificateOK(); });
  QObject::connect(verifyDialog, &QDialog::rejected,
                   [this]() { this->handleCertificateCancel(); });

  verifyDialog->show();
}

void CConn::handleCertificateCancel()
{
  assert(verifyDialog);

  verifyDialog->deleteLater();
  verifyDialog = nullptr;

  vlog.info(_("Authentication cancelled"));
  disconnect();
}
#endif

void CConn::handleHostKeyOK()
{
  assert(verifyDialog);

  verifyDialog->deleteLater();
  verifyDialog = nullptr;

  approveHostKey();
  resumeProcessing();
}

void CConn::handleHostKeyCancel()
{
  assert(verifyDialog);

  verifyDialog->deleteLater();
  verifyDialog = nullptr;

  vlog.info(_("Authentication cancelled"));
  disconnect();
}

void CConn::resumeProcessing()
{
  socketEvent();
}
