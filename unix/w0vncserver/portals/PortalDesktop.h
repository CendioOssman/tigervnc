/* Copyright 2025 Adam Halim for Cendio AB
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

#ifndef __PORTAL_DESKTOP_H__
#define __PORTAL_DESKTOP_H__

#include <glib.h>

#include <string>

#include <rfb/SDesktop.h>

class PipeWirePixelBuffer;

namespace rfb {
  struct KeyEvent;
  struct PointerEvent;
  class VNCServer;
}

class RemoteDesktop;

class PortalDesktop : public rfb::SDesktop
{
public:
  PortalDesktop();
  virtual ~PortalDesktop();

  // -=- SDesktop interface
  void init(rfb::VNCServer* vs) override;
  void queryConnection(network::Socket* sock,
                       const char* userName) override;

  // Check if portals implementations are available
  static bool available();

protected:
  // Signal handlers
  void start();
  void stop();
  void keyEvent(rfb::VNCServer*, const char* name,
                rfb::KeyEvent event);
  void pointerEvent(rfb::PointerEvent event);

protected:
  rfb::VNCServer* server;

  RemoteDesktop* remoteDesktop;
  PipeWirePixelBuffer* pb;

private:
  std::string restoreToken;
};

#endif // __PORTAL_DESKTOP_H__
