/* Copyright 2015 Pierre Ossman for Cendio AB
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

#ifndef DECODEMANAGER_H
#define DECODEMANAGER_H

#include <list>

#include <os/Thread.h>

#include <rfb/Region.h>
#include <rfb/encodings.h>

namespace os {
  class Condition;
  class Mutex;
}

namespace rdr {
  struct Exception;
  class MemOutStream;
}

namespace rfb {
  class Decoder;
  struct Rect;
  class ServerParams;
  class ModifiablePixelBuffer;
}

class QVNCConnection;

class DecodeManager {
public:
  DecodeManager(QVNCConnection *conn);
  ~DecodeManager();

  bool decodeRect(const rfb::Rect& r, int encoding, rfb::ModifiablePixelBuffer* pb);

  void flush();

private:
  void logStats();

  void setThreadException(const rdr::Exception& e);
  void throwThreadException();

private:
  QVNCConnection *conn;
  rfb::Decoder *decoders[rfb::encodingMax+1];

  struct DecoderStats {
    unsigned rects;
    unsigned long long bytes;
    unsigned long long pixels;
    unsigned long long equivalent;
  };

  DecoderStats stats[rfb::encodingMax+1];

  struct QueueEntry {
    bool active;
    rfb::Rect rect;
    int encoding;
    rfb::Decoder* decoder;
    const rfb::ServerParams* server;
    rfb::ModifiablePixelBuffer* pb;
    rdr::MemOutStream* bufferStream;
    rfb::Region affectedRegion;
  };

  std::list<rdr::MemOutStream*> freeBuffers;
  std::list<QueueEntry*> workQueue;

  os::Mutex* queueMutex;
  os::Condition* producerCond;
  os::Condition* consumerCond;

private:
  class DecodeThread : public os::Thread {
  public:
    DecodeThread(DecodeManager* manager);
    ~DecodeThread();

    void stop();

  protected:
    void worker();
    DecodeManager::QueueEntry* findEntry();

  private:
    DecodeManager* manager;

    bool stopRequested;
  };

  std::list<DecodeThread*> threads;
  rdr::Exception *threadException;
};

#endif
