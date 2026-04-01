/* Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#ifndef __MAINLOOP_H__
#define __MAINLOOP_H__

namespace rdr {
  struct Exception;
};

namespace network {
  class Socket;
};

// Report a fatal issue that requires us to terminate all of vncviewer
void abort_vncviewer(const char *error, ...)
  __attribute__((__format__ (__printf__, 1, 2)));
// Report an issue that forces us to terminated the connection, but
// still permits reconnecting
void abort_connection(const char *error, ...)
  __attribute__((__format__ (__printf__, 1, 2)));
// Convenience version of abort_connection() for unexpected exceptions
void abort_connection_unexpected(const rdr::Exception &);
// Or other unexpected errors
void abort_connection_unexpected(const char *error, ...)
  __attribute__((__format__ (__printf__, 1, 2)));

// Cleanly terminate the connection
void disconnect();

void mainloop(const char* vncserver, network::Socket* sock);

#endif
