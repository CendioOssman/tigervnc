/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
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

#ifndef __CCONN_H__
#define __CCONN_H__

#include <rfb/CConnection.h>

namespace network { class Socket; }

class QSocketNotifier;
class QTimer;

class DesktopWindow;
class AuthDialog;

class CConn : public rfb::CConnection
{
public:
  CConn(const char* vncServerName, network::Socket* sock);
  ~CConn();

  const char *connectionInfo();

  unsigned getUpdateCount();
  unsigned getPixelCount();
  unsigned getPosition();

  // CConnection callback methods

  void credentialsRequested(bool secure, bool needsUser,
                            bool needsPassword) override;
  bool verifyCertificate(unsigned int status,
                         const uint8_t* certificate,
                         size_t length) override;
  bool verifyHostKey(const uint8_t* key, size_t length,
                     const char* fingerprint) override;
  bool showMsgBox(rfb::MsgBoxFlags flags, const char *title,
                  const char *text) override;

  void initDone() override;

  void setDesktopSize(int w, int h) override;
  void setExtendedDesktopSize(unsigned reason, unsigned result,
                              int w, int h,
                              const rfb::ScreenSet& layout) override;

  void setName(const char* name) override;

  void setColourMapEntries(int firstColour, int nColours,
                           uint16_t* rgbs) override;

  void bell() override;

  void framebufferUpdateStart() override;
  void framebufferUpdateEnd() override;
  bool dataRect(const rfb::Rect& r, int encoding) override;

  void setCursor(int width, int height, const rfb::Point& hotspot,
                 const uint8_t* data) override;
  void setCursorPos(const rfb::Point& pos) override;

  void fence(uint32_t flags, unsigned len,
             const uint8_t data[]) override;

  void setLEDState(unsigned int state) override;

  void handleClipboardRequest() override;
  void handleClipboardAnnounce(bool available) override;
  void handleClipboardData(const char* data) override;

  rfb::ModifiablePixelBuffer* framebuffer(); // public facade for the protected method.

private:

  void resizeFramebuffer() override;

  void autoSelectFormatAndEncoding();
  void updatePixelFormat();

  void startProcessing();
  void flushSocket();
  void processNextMsg();

  static void handleOptions(void *data);

  void handleUpdateTimeout(rfb::Timer*);

  void handleAuthOK();
  void handleAuthCancel();

private:
  std::string serverHost;
  int serverPort;
  network::Socket* sock;
  QSocketNotifier* socketReadNotifier;
  QSocketNotifier* socketWriteNotifier;
  QTimer* processTimer;

  DesktopWindow *desktop;

  unsigned updateCount;
  unsigned pixelCount;

  rfb::PixelFormat serverPF;
  rfb::PixelFormat fullColourPF;

  int lastServerEncoding;

  struct timeval updateStartTime;
  size_t updateStartPos;
  unsigned long long bpsEstimate;

  static std::string savedUsername;
  static std::string savedPassword;

  AuthDialog* authDialog;

  rfb::MethodTimer<CConn> updateTimer;
};

#endif
