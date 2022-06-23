/* Copyright 2024-2025 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

class Sender : public core::Object {
public:
  Sender() {}

  core::signal gsignal;
  core::signal gsignal2;

  void emitSignal(core::signal& signal)
  {
    core::Object::emitSignal(signal);
  }
};

class Receiver : public core::Object {
public:
  Receiver() {}

  void genericHandler(Object*) { callCount++; }
  void otherGenericHandler(Object*) { callCount++; }

  void registerHandler(Object* s)
  {
    s->connectSignal(((Sender*)s)->gsignal, this, &Receiver::genericHandler);
  }
  void unregisterHandler(Object* s)
  {
    s->disconnectSignal(((Sender*)s)->gsignal, this, &Receiver::genericHandler);
  }
};

TEST(Signals, connectSignal)
{
  Sender s;
  Receiver r;

  /* Generic handler */
  callCount = 0;
  s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.emitSignal(s.gsignal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, doubleConnect)
{
  Sender s;
  Receiver r;

  s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  EXPECT_THROW({
    s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  }, std::logic_error);
}

TEST(Signals, disconnectSignal)
{
  Sender s;
  Receiver r;
  core::Connection c;

  /* Generic handler */
  callCount = 0;
  c = s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.disconnectSignal(c);
  s.emitSignal(s.gsignal);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, doubleDisconnect)
{
  Sender s;
  Receiver r;
  core::Connection c;

  c = s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.disconnectSignal(c);
  EXPECT_NO_THROW({
    s.disconnectSignal(c);
  });
}

TEST(Signals, disconnectWrongObject)
{
  Sender s;
  Sender s2;
  Receiver r;
  core::Connection c;

  c = s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  EXPECT_THROW({
    s2.disconnectSignal(c);
  }, std::logic_error);
}

TEST(Signals, disconnectHelper)
{
  Sender s;
  Receiver r;

  /* Generic handler */
  callCount = 0;
  s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.disconnectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.emitSignal(s.gsignal);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, disconnectSimilar)
{
  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.connectSignal(s.gsignal, &r, &Receiver::otherGenericHandler);
  s.disconnectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.emitSignal(s.gsignal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, disconnectAll)
{
  Sender s;
  Receiver r;
  Receiver r2;

  callCount = 0;
  s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.connectSignal(s.gsignal2, &r, &Receiver::genericHandler);
  s.connectSignal(s.gsignal, &r2, &Receiver::genericHandler);
  s.disconnectSignals(&r);
  s.emitSignal(s.gsignal);
  s.emitSignal(s.gsignal2);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, implicitDisconnect)
{
  Sender s;

  callCount = 0;
  {
    Receiver scoped_r;
    s.connectSignal(s.gsignal, &scoped_r, &Receiver::genericHandler);
  }
  s.emitSignal(s.gsignal);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, addWhileEmitting)
{
  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(s.gsignal, &r, &Receiver::registerHandler);
  s.connectSignal(s.gsignal, &r, &Receiver::otherGenericHandler);
  s.emitSignal(s.gsignal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, removeWhileEmitting)
{
  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(s.gsignal, &r, &Receiver::unregisterHandler);
  s.connectSignal(s.gsignal, &r, &Receiver::genericHandler);
  s.connectSignal(s.gsignal, &r, &Receiver::otherGenericHandler);
  s.emitSignal(s.gsignal);
  EXPECT_EQ(callCount, 1);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
