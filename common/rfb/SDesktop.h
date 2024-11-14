/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2009-2024 Pierre Ossman for Cendio AB
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

/////////////////////////////////////////////////////////////////////////////

// SDesktop is an interface implemented by back-ends, on which callbacks are
// made by the VNCServer as appropriate for pointer and keyboard events, etc.
// SDesktop objects are always created before the VNCServer - the SDesktop
// will be passed a pointer to the VNCServer in the start() call.  If a more
// implementation-specific pointer to the VNCServer is required then this
// can be provided to the SDesktop via an implementation-specific method.
//
// An SDesktop usually has an associated PixelBuffer which it tells the
// VNCServer via the VNCServer's setPixelBuffer() method.  It can do this at
// any time, but the PixelBuffer MUST be valid by the time the call to start()
// returns.  The PixelBuffer may be set to null again if desired when stop() is
// called.  Note that start() and stop() are guaranteed to be called
// alternately; there should never be two calls to start() without an
// intervening stop() and vice-versa.
//

#ifndef __RFB_SDESKTOP_H__
#define __RFB_SDESKTOP_H__

#include <core/Object.h>

#include <rfb/screenTypes.h>

namespace network { class Socket; }

namespace rfb {

  struct ScreenSet;
  class VNCServer;

  class SDesktop : public core::Object {
  public:
    // init() is called immediately when the VNCServer gets a reference
    // to the SDesktop, so that a reverse reference can be set up.
    virtual void init(rfb::VNCServer* vs) = 0;

    // queryConnection() is called when a connection has been
    // successfully authenticated.  The sock and userName arguments
    // identify the socket and the name of the authenticated user, if
    // any. At some point later VNCServer::approveConnection() should
    // be called to either accept or reject the client.
    virtual void queryConnection(network::Socket* sock,
                                 const char* userName) = 0;

    // terminate() is called by the server when it wishes to terminate
    // itself, e.g. because it was configured to terminate when no one is
    // using it.

    virtual void terminate() = 0;

    // setScreenLayout() requests to reconfigure the framebuffer and/or
    // the layout of screens.
    virtual unsigned int setScreenLayout(int /*fb_width*/,
                                         int /*fb_height*/,
                                         const ScreenSet& /*layout*/) {
      return resultProhibited;
    }

    // getLEDState() returns the current lock keys LED state
    virtual unsigned int getLEDState() = 0;

    // Signals

    // "ledstate" is emitted when the current lock keys LED state
    // changes.

  protected:
    virtual ~SDesktop() {}
  };

};

#endif // __RFB_SDESKTOP_H__
