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

  void registerSignal(const char* name)
  {
    core::Object::registerSignal(name);
  }
  void emitSignal(const char* name)
  {
    core::Object::emitSignal(name);
  }
};

class Receiver : public core::Object {
public:
  Receiver() {}

  void genericHandler(Object*, const char*) { callCount++; }
  void otherGenericHandler(Object*, const char*) { callCount++; }
};

TEST(Signals, doubleRegister)
{
  Sender s;

  s.registerSignal("signal");
  EXPECT_THROW({
    s.registerSignal("signal");
  }, std::logic_error);
}

TEST(Signals, emitUnknown)
{
  Sender s;

  EXPECT_THROW({
    s.emitSignal("nosignal");
  }, std::logic_error);
}

TEST(Signals, connectSignal)
{
  Sender s;
  Receiver r;

  /* Generic handler */
  callCount = 0;
  s.registerSignal("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.emitSignal("gsignal");
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, connectUnknown)
{
  Sender s;
  Receiver r;

  EXPECT_THROW({
    s.connectSignal("nosignal", &r, &Receiver::genericHandler);
  }, std::logic_error);
}

TEST(Signals, doubleConnect)
{
  Sender s;
  Receiver r;

  s.registerSignal("dblsignal");
  s.connectSignal("dblsignal", &r, &Receiver::genericHandler);
  EXPECT_THROW({
    s.connectSignal("dblsignal", &r, &Receiver::genericHandler);
  }, std::logic_error);
}

TEST(Signals, disconnectSignal)
{
  Sender s;
  Receiver r;
  core::Connection c;

  /* Generic handler */
  callCount = 0;
  s.registerSignal("gsignal");
  c = s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal(c);
  s.emitSignal("gsignal");
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, doubleDisconnect)
{
  Sender s;
  Receiver r;
  core::Connection c;

  s.registerSignal("dblsignal");
  c = s.connectSignal("dblsignal", &r, &Receiver::genericHandler);
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

  s.registerSignal("othersignal");
  c = s.connectSignal("othersignal", &r, &Receiver::genericHandler);
  EXPECT_THROW({
    s2.disconnectSignal(c);
  }, std::logic_error);
}

TEST(Signals, disconnectWrongSignal)
{
  Sender s;
  Receiver r;
  core::Connection c;

  /* Incorrect signal name (should be impossible) */
  s.registerSignal("renamesignal");
  c = s.connectSignal("renamesignal", &r, &Receiver::genericHandler);
  c.name = "badname";
  EXPECT_THROW({
    s.disconnectSignal(c);
  }, std::logic_error);
}

TEST(Signals, disconnectHelper)
{
  Sender s;
  Receiver r;

  /* Generic handler */
  callCount = 0;
  s.registerSignal("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal("gsignal", &r, &Receiver::genericHandler);
  s.emitSignal("gsignal");
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, disconnectSimilar)
{
  Sender s;
  Receiver r;

  callCount = 0;
  s.registerSignal("osignal");
  s.connectSignal("osignal", &r, &Receiver::genericHandler);
  s.connectSignal("osignal", &r, &Receiver::otherGenericHandler);
  s.disconnectSignal("osignal", &r, &Receiver::genericHandler);
  s.emitSignal("osignal");
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, disconnectUnknown)
{
  Sender s;
  Receiver r;

  EXPECT_THROW({
    s.disconnectSignal("nosignal", &r, &Receiver::genericHandler);
  }, std::logic_error);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
