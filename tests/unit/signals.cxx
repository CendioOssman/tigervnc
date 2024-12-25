/* Copyright 2024 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <stdio.h>

#include <core/Object.h>

static unsigned count;

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

  void genericHandler(Object*, const char*) { count++; }
  void otherGenericHandler(Object*, const char*) { count++; }
};

#define ASSERT_EQ(expr, val) if ((expr) != (val)) { \
  printf("FAILED on line %d (%s equals %d, expected %d)\n", __LINE__, #expr, (int)(expr), (int)(val)); \
  return; \
}

static void testRegister()
{
  Sender s;
  bool ok;

  printf("%s: ", __func__);

  /* Double register */
  s.registerSignal("signal");
  ok = false;
  try {
    s.registerSignal("signal");
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  printf("OK\n");
}

static void testEmit()
{
  Sender s;
  bool ok;

  printf("%s: ", __func__);

  /* Unknown signal */
  ok = false;
  try {
    s.emitSignal("nosignal");
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  printf("OK\n");
}

static void testConnect()
{
  Sender s;
  Receiver r;
  bool ok;

  printf("%s: ", __func__);

  /* Generic handler */
  count = 0;
  s.registerSignal("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.emitSignal("gsignal");
  ASSERT_EQ(count, 1);

  /* Unknown signal */
  ok = false;
  try {
    s.connectSignal("nosignal", &r, &Receiver::genericHandler);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  /* Double connect */
  ok = false;
  try {
    s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  printf("OK\n");
}

static void testDisconnect()
{
  Sender s;
  Sender s2;
  Receiver r;
  core::Connection c;
  bool ok;

  printf("%s: ", __func__);

  /* Generic handler */
  count = 0;
  s.registerSignal("gsignal");
  c = s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal(c);
  s.emitSignal("gsignal");
  ASSERT_EQ(count, 0);

  /* Double remove */
  ok = true;
  s.registerSignal("dblsignal");
  c = s.connectSignal("dblsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal(c);
  try {
    s.disconnectSignal(c);
  } catch (std::exception&) {
    ok = false;
  }
  ASSERT_EQ(ok, true);

  /* Wrong object */
  ok = false;
  s.registerSignal("othersignal");
  c = s.connectSignal("othersignal", &r, &Receiver::genericHandler);
  try {
    s2.disconnectSignal(c);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  /* Incorrect signal name (should be impossible) */
  ok = false;
  s.registerSignal("renamesignal");
  c = s.connectSignal("renamesignal", &r, &Receiver::genericHandler);
  c.name = "badname";
  try {
    s.disconnectSignal(c);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  printf("OK\n");
}

static void testDisconnectHelper()
{
  Sender s;
  Receiver r;
  bool ok;

  printf("%s: ", __func__);

  /* Generic handler */
  count = 0;
  s.registerSignal("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal("gsignal", &r, &Receiver::genericHandler);
  s.emitSignal("gsignal");
  ASSERT_EQ(count, 0);

  /* Similar handlers */
  count = 0;
  s.registerSignal("osignal");
  s.connectSignal("osignal", &r, &Receiver::genericHandler);
  s.connectSignal("osignal", &r, &Receiver::otherGenericHandler);
  s.disconnectSignal("osignal", &r, &Receiver::genericHandler);
  s.emitSignal("osignal");
  ASSERT_EQ(count, 1);

  /* Unknown signal */
  ok = false;
  try {
    s.disconnectSignal("nosignal", &r, &Receiver::genericHandler);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  printf("OK\n");
}

int main(int /*argc*/, char** /*argv*/)
{
  testRegister();
  testEmit();
  testConnect();
  testDisconnect();
  testDisconnectHelper();

  return 0;
}
