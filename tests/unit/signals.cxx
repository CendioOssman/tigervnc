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
  template<typename I>
  void registerSignal(const char* name)
  {
    core::Object::registerSignal<I>(name);
  }

  void emitSignal(const char* name)
  {
    core::Object::emitSignal(name);
  }
  template<typename I>
  void emitSignal(const char* name, I info)
  {
    core::Object::emitSignal(name, info);
  }
};

class Receiver : public core::Object {
public:
  Receiver() {}

  void simpleHandler() { count++; }
  void genericHandler(Object*, const char*) { count++; }
  void otherGenericHandler(Object*, const char*) { count++; }
  void specificHandler(Sender*, const char*) { count++; }

  void simpleStringHandler(const char*) { count++; }
  void genericStringHandler(Object*, const char*, const char*) { count++; }
  void otherGenericStringHandler(Object*, const char*, const char*) { count++; }
  void specificStringHandler(Sender*, const char*, const char*) { count++; }

  void simpleConstPtrHandler(const int*) { count++; }
  void simpleConstRefHandler(const int&) { count++; }
  void genericConstPtrHandler(Object*, const char*, const int*) { count++; }
  void genericConstRefHandler(Object*, const char*, const int&) { count++; }

  void registerHandler(Object* s, const char* signal)
  {
    s->connectSignal(signal, this, &Receiver::genericHandler);
  }
  void unregisterHandler(Object* s, const char* signal)
  {
    s->disconnectSignal(signal, this, &Receiver::genericHandler);
  }
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

static void testRegisterArg()
{
  Sender s;
  bool ok;

  printf("%s: ", __func__);

  /* Double register */
  s.registerSignal<const char*>("signal");
  ok = false;
  try {
    s.registerSignal<const char*>("signal");
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

  /* Unexpected data */
  s.registerSignal("signal");
  ok = false;
  try {
    s.emitSignal("signal", "data");
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  printf("OK\n");
}

static void testEmitArg()
{
  Sender s;
  bool ok;

  printf("%s: ", __func__);

  /* Unknown signal */
  ok = false;
  try {
    s.emitSignal("nosignal", "data");
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  /* Missing data */
  s.registerSignal<const char*>("signal");
  ok = false;
  try {
    s.emitSignal("signal");
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  /* Wrong data */
  ok = false;
  try {
    s.emitSignal("signal", 1234);
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

  /* Simple handler */
  count = 0;
  s.registerSignal("signal");
  s.connectSignal("signal", &r, &Receiver::simpleHandler);
  s.emitSignal("signal");
  ASSERT_EQ(count, 1);

  /* Generic handler */
  count = 0;
  s.registerSignal("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.emitSignal("gsignal");
  ASSERT_EQ(count, 1);

  /* Specific handler */
  count = 0;
  s.registerSignal("ssignal");
  s.connectSignal("ssignal", &r, &Receiver::specificHandler);
  s.emitSignal("ssignal");
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

  /* Unexpected data */
  s.registerSignal("nodata");
  ok = false;
  try {
    s.connectSignal("nodata", &r, &Receiver::genericStringHandler);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  printf("OK\n");
}

static void testConnectArg()
{
  Sender s;
  Receiver r;
  bool ok;

  printf("%s: ", __func__);

  /* Simple handler */
  count = 0;
  s.registerSignal<const char*>("signal");
  s.connectSignal("signal", &r, &Receiver::simpleStringHandler);
  s.emitSignal("signal", "data");
  ASSERT_EQ(count, 1);

  /* Generic handler */
  count = 0;
  s.registerSignal<const char*>("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericStringHandler);
  s.emitSignal("gsignal", "data");
  ASSERT_EQ(count, 1);

  /* Specific handler */
  count = 0;
  s.registerSignal<const char*>("ssignal");
  s.connectSignal("ssignal", &r, &Receiver::specificStringHandler);
  s.emitSignal("ssignal", "data");
  ASSERT_EQ(count, 1);

  /* Unknown signal */
  ok = false;
  try {
    s.connectSignal("nosignal", &r, &Receiver::genericStringHandler);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  /* Double connect */
  ok = false;
  try {
    s.connectSignal("gsignal", &r, &Receiver::genericStringHandler);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  /* Missing data */
  s.registerSignal<int>("withdata");
  ok = false;
  try {
    s.connectSignal("withdata", &r, &Receiver::genericHandler);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  /* Wrong data type */
  ok = false;
  try {
    s.connectSignal("withdata", &r, &Receiver::genericStringHandler);
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

  /* Simple handler */
  count = 0;
  s.registerSignal("signal");
  c = s.connectSignal("signal", &r, &Receiver::simpleHandler);
  s.disconnectSignal(c);
  s.emitSignal("signal");
  ASSERT_EQ(count, 0);

  /* Generic handler */
  count = 0;
  s.registerSignal("gsignal");
  c = s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal(c);
  s.emitSignal("gsignal");
  ASSERT_EQ(count, 0);

  /* Specific handler */
  count = 0;
  s.registerSignal("ssignal");
  c = s.connectSignal("ssignal", &r, &Receiver::specificHandler);
  s.disconnectSignal(c);
  s.emitSignal("ssignal");
  ASSERT_EQ(count, 0);

  /* Simple handler with args */
  count = 0;
  s.registerSignal<const char*>("asignal");
  c = s.connectSignal("asignal", &r, &Receiver::simpleStringHandler);
  s.disconnectSignal(c);
  s.emitSignal("asignal", "data");
  ASSERT_EQ(count, 0);

  /* Generic handler with args */
  count = 0;
  s.registerSignal<const char*>("gasignal");
  c = s.connectSignal("gasignal", &r, &Receiver::genericStringHandler);
  s.disconnectSignal(c);
  s.emitSignal("gasignal", "data");
  ASSERT_EQ(count, 0);

  /* Specific handler with args */
  count = 0;
  s.registerSignal<const char*>("sasignal");
  c = s.connectSignal("sasignal", &r, &Receiver::specificStringHandler);
  s.disconnectSignal(c);
  s.emitSignal("sasignal", "data");
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

  /* Simple handler */
  count = 0;
  s.registerSignal("signal");
  s.connectSignal("signal", &r, &Receiver::simpleHandler);
  s.disconnectSignal("signal", &r, &Receiver::simpleHandler);
  s.emitSignal("signal");
  ASSERT_EQ(count, 0);

  /* Generic handler */
  count = 0;
  s.registerSignal("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal("gsignal", &r, &Receiver::genericHandler);
  s.emitSignal("gsignal");
  ASSERT_EQ(count, 0);

  /* Specific handler */
  count = 0;
  s.registerSignal("ssignal");
  s.connectSignal("ssignal", &r, &Receiver::specificHandler);
  s.disconnectSignal("ssignal", &r, &Receiver::specificHandler);
  s.emitSignal("ssignal");
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

static void testDisconnectHelperArg()
{
  Sender s;
  Receiver r;
  bool ok;

  printf("%s: ", __func__);

  /* Simple handler */
  count = 0;
  s.registerSignal<const char*>("signal");
  s.connectSignal("signal", &r, &Receiver::simpleStringHandler);
  s.disconnectSignal("signal", &r, &Receiver::simpleStringHandler);
  s.emitSignal("signal", "data");
  ASSERT_EQ(count, 0);

  /* Generic handler */
  count = 0;
  s.registerSignal<const char*>("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericStringHandler);
  s.disconnectSignal("gsignal", &r, &Receiver::genericStringHandler);
  s.emitSignal("gsignal", "data");
  ASSERT_EQ(count, 0);

  /* Specific handler */
  count = 0;
  s.registerSignal<const char*>("ssignal");
  s.connectSignal("ssignal", &r, &Receiver::specificStringHandler);
  s.disconnectSignal("ssignal", &r, &Receiver::specificStringHandler);
  s.emitSignal("ssignal", "data");
  ASSERT_EQ(count, 0);

  /* Similar handlers */
  count = 0;
  s.registerSignal<const char*>("osignal");
  s.connectSignal("osignal", &r, &Receiver::genericStringHandler);
  s.connectSignal("osignal", &r, &Receiver::otherGenericStringHandler);
  s.disconnectSignal("osignal", &r, &Receiver::genericStringHandler);
  s.emitSignal("osignal", "data");
  ASSERT_EQ(count, 1);

  /* Unknown signal */
  ok = false;
  try {
    s.disconnectSignal("nosignal", &r, &Receiver::genericStringHandler);
  } catch (std::exception&) {
    ok = true;
  }
  ASSERT_EQ(ok, true);

  printf("OK\n");
}

static void testDisconnectAll()
{
  Sender s;
  Receiver r;
  Receiver r2;

  printf("%s: ", __func__);

  /* Disconnect all signals */
  count = 0;
  s.registerSignal("signal1");
  s.registerSignal("signal2");
  s.registerSignal<const char*>("signal3");
  s.connectSignal("signal1", &r, &Receiver::simpleHandler);
  s.connectSignal("signal1", &r, &Receiver::genericHandler);
  s.connectSignal("signal2", &r, &Receiver::genericHandler);
  s.connectSignal("signal3", &r, &Receiver::simpleStringHandler);
  s.connectSignal("signal3", &r, &Receiver::genericStringHandler);
  s.connectSignal("signal1", &r2, &Receiver::genericHandler);
  s.disconnectSignals(&r);
  s.emitSignal("signal1");
  s.emitSignal("signal2");
  s.emitSignal("signal3", "data");
  ASSERT_EQ(count, 1);

  /* Implicit disconnect */
  count = 0;
  {
    Receiver scoped_r;
    s.registerSignal("gsignal");
    s.connectSignal("gsignal", &scoped_r, &Receiver::genericHandler);
  }
  s.emitSignal("gsignal");
  ASSERT_EQ(count, 0);

  printf("OK\n");
}

static void testChangesWhileEmitting()
{
  Sender s;
  Receiver r;

  printf("%s: ", __func__);

  /* Receivers getting added */
  count = 0;
  s.registerSignal("asignal");
  s.connectSignal("asignal", &r, &Receiver::registerHandler);
  s.connectSignal("asignal", &r, &Receiver::otherGenericHandler);
  s.emitSignal("asignal");
  ASSERT_EQ(count, 1);

  /* Receivers getting removed */
  count = 0;
  s.registerSignal("rsignal");
  s.connectSignal("rsignal", &r, &Receiver::unregisterHandler);
  s.connectSignal("rsignal", &r, &Receiver::genericHandler);
  s.connectSignal("rsignal", &r, &Receiver::otherGenericHandler);
  s.emitSignal("rsignal");
  ASSERT_EQ(count, 1);

  printf("OK\n");
}

static void testEmitConversion()
{
  Sender s;
  Receiver r;

  printf("%s: ", __func__);

  /* Receiver adds reference */
  count = 0;
  s.registerSignal<int>("refhandler");
  s.connectSignal("refhandler", &r, &Receiver::simpleConstRefHandler);
  s.connectSignal("refhandler", &r, &Receiver::genericConstRefHandler);
  s.emitSignal("refhandler", 123);
  ASSERT_EQ(count, 2);

  /* Sender adds reference */
  count = 0;
  s.registerSignal<const int&>("refemitter");
  s.connectSignal("refemitter", &r, &Receiver::simpleConstRefHandler);
  s.connectSignal("refemitter", &r, &Receiver::genericConstRefHandler);
  s.emitSignal("refemitter", 123);
  ASSERT_EQ(count, 2);

  /* Receiver adds pointer const qualifier */
#if 0 /* FIXME: Currently broken*/
  count = 0;
  s.registerSignal<int*>("constemitter");
  s.connectSignal("constemitter", &r, &Receiver::simpleConstPtrHandler);
  s.connectSignal("constemitter", &r, &Receiver::genericConstPtrHandler);
  s.emitSignal("constemitter", &count);
  ASSERT_EQ(count, 2);
#endif

  printf("OK\n");
}

int main(int /*argc*/, char** /*argv*/)
{
  testRegister();
  testRegisterArg();
  testEmit();
  testEmitArg();
  testConnect();
  testConnectArg();
  testDisconnect();
  testDisconnectHelper();
  testDisconnectHelperArg();
  testDisconnectAll();
  testChangesWhileEmitting();
  testEmitConversion();

  return 0;
}
