/* Copyright 2024-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <gtest/gtest.h>

#include <core/Object.h>

static unsigned callCount;

class SenderBase : public core::Object {
public:
  SenderBase() {}

  template<class S>
  void emitSignal(const core::signal S::* signal)
  {
    core::Object::emitSignal(signal);
  }
};

class Receiver : public core::Object {
public:
  Receiver() {}

  void handler() { callCount++; }
  void otherHandler() { callCount++; }
};

TEST(Signals, connectSignal)
{
  class Sender : public SenderBase {
  public:
    core::signal signal;
  };

  Sender s;
  Receiver r;

  /* Normal handler */
  callCount = 0;
  s.connectSignal(&Sender::signal, &r, &Receiver::handler);
  s.emitSignal(&Sender::signal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, connectBadSignal)
{
  class SenderA : public SenderBase {
  public:
    core::signal goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal badsignal;
  };

  SenderA s;
  Receiver r;

  s.connectSignal(&SenderA::goodsignal, &r, &Receiver::handler);
  EXPECT_THROW({
    s.connectSignal(&SenderB::badsignal, &r, &Receiver::handler);
  }, std::logic_error);
}

TEST(Signals, doubleConnect)
{
  class Sender : public SenderBase {
  public:
    core::signal dblsignal;
  };

  Sender s;
  Receiver r;

  s.connectSignal(&Sender::dblsignal, &r, &Receiver::handler);
  EXPECT_THROW({
    s.connectSignal(&Sender::dblsignal, &r, &Receiver::handler);
  }, std::logic_error);
}

TEST(Signals, disconnectSignal)
{
  class Sender : public SenderBase {
  public:
    core::signal signal;
  };

  Sender s;
  Receiver r;
  core::Connection c;

  /* Normal handler */
  callCount = 0;
  c = s.connectSignal(&Sender::signal, &r, &Receiver::handler);
  s.disconnectSignal(c);
  s.emitSignal(&Sender::signal);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, doubleDisconnect)
{
  class Sender : public SenderBase {
  public:
    core::signal dblsignal;
  };

  Sender s;
  Receiver r;
  core::Connection c;

  c = s.connectSignal(&Sender::dblsignal, &r, &Receiver::handler);
  s.disconnectSignal(c);
  EXPECT_NO_THROW({
    s.disconnectSignal(c);
  });
}

TEST(Signals, disconnectWrongObject)
{
  class Sender : public SenderBase {
  public:
    core::signal othersignal;
  };

  Sender s;
  Sender s2;
  Receiver r;
  core::Connection c;

  c = s.connectSignal(&Sender::othersignal, &r, &Receiver::handler);
  EXPECT_THROW({
    s2.disconnectSignal(c);
  }, std::logic_error);
}

TEST(Signals, disconnectHelper)
{
  class Sender : public SenderBase {
  public:
    core::signal signal;
  };

  Sender s;
  Receiver r;

  /* Normal handler */
  callCount = 0;
  s.connectSignal(&Sender::signal, &r, &Receiver::handler);
  s.disconnectSignal(&Sender::signal, &r, &Receiver::handler);
  s.emitSignal(&Sender::signal);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, disconnectSimilar)
{
  class Sender : public SenderBase {
  public:
    core::signal osignal;
  };

  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(&Sender::osignal, &r, &Receiver::handler);
  s.connectSignal(&Sender::osignal, &r, &Receiver::otherHandler);
  s.disconnectSignal(&Sender::osignal, &r, &Receiver::handler);
  s.emitSignal(&Sender::osignal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, disconnectBadSignal)
{
  class SenderA : public SenderBase {
  public:
    core::signal goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal badsignal;
  };

  SenderA s;
  Receiver r;

  s.disconnectSignal(&SenderA::goodsignal, &r, &Receiver::handler);
  EXPECT_THROW({
    s.disconnectSignal(&SenderB::badsignal, &r, &Receiver::handler);
  }, std::logic_error);
}

TEST(Signals, addWhileEmitting)
{
  class Sender : public SenderBase {
  public:
    core::signal asignal;
  };
  class AddReceiver : public Receiver {
  public:
    AddReceiver(Sender* s) : sender(s) {}
    void registerHandler()
    {
      sender->connectSignal(&Sender::asignal, this,
                            &AddReceiver::handler);
    }
    void handler() { callCount++; }
    void otherHandler() { callCount++; }
  protected:
    Sender* sender;
  };

  Sender s;
  AddReceiver r(&s);

  callCount = 0;
  s.connectSignal(&Sender::asignal, &r, &AddReceiver::registerHandler);
  s.connectSignal(&Sender::asignal, &r, &AddReceiver::otherHandler);
  s.emitSignal(&Sender::asignal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, removeWhileEmitting)
{
  class Sender : public SenderBase {
  public:
    core::signal rsignal;
  };

  class RemoveReceiver : public Receiver {
  public:
    RemoveReceiver(Sender* s) : sender(s) {}
    void unregisterHandler()
    {
      sender->disconnectSignal(&Sender::rsignal, this,
                               &RemoveReceiver::handler);
    }
    void handler() { callCount++; }
    void otherHandler() { callCount++; }
  protected:
    Sender* sender;
  };

  Sender s;
  RemoveReceiver r(&s);

  callCount = 0;
  s.connectSignal(&Sender::rsignal, &r,
                  &RemoveReceiver::unregisterHandler);
  s.connectSignal(&Sender::rsignal, &r, &RemoveReceiver::handler);
  s.connectSignal(&Sender::rsignal, &r, &RemoveReceiver::otherHandler);
  s.emitSignal(&Sender::rsignal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, emitBadSignal)
{
  class SenderA : public SenderBase {
  public:
    core::signal goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal badsignal;
  };

  SenderA s;

  s.emitSignal(&SenderA::goodsignal);
  EXPECT_THROW({
    s.emitSignal(&SenderB::badsignal);
  }, std::logic_error);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
