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

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
