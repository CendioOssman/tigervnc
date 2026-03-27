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
  template<typename... Args>
  void registerSignal(const char* name)
  {
    core::Object::registerSignal<Args...>(name);
  }

  void emitSignal(const char* name)
  {
    core::Object::emitSignal(name);
  }
  template<typename... Args>
  void emitSignal(const char* name, Args... args)
  {
    core::Object::emitSignal(name, args...);
  }
};

class Receiver : public core::Object {
public:
  Receiver() {}

  void simpleHandler() { callCount++; }
  void genericHandler(Object*, const char*) { callCount++; }
  void otherGenericHandler(Object*, const char*) { callCount++; }
  void specificHandler(Sender*, const char*) { callCount++; }
  void badSpecificHandler(Receiver*, const char*) { callCount++; }

  template<typename... Args>
  void simpleTypeHandler(Args...) { callCount++; }
  template<typename... Args>
  void genericTypeHandler(Object*, const char*, Args...) { callCount++; }
  template<typename... Args>
  void otherGenericTypeHandler(Object*, const char*, Args...) { callCount++; }
  template<typename... Args>
  void specificTypeHandler(Sender*, const char*, Args...) { callCount++; }
  template<typename... Args>
  void badSpecificTypeHandler(Receiver*, const char*, Args...) { callCount++; }

  void registerHandler(Object* s, const char* signal)
  {
    s->connectSignal(signal, this, &Receiver::genericHandler);
  }
  void unregisterHandler(Object* s, const char* signal)
  {
    s->disconnectSignal(signal, this, &Receiver::genericHandler);
  }
};

template<typename T>
class SignalsArgs : public testing::Test {
public:
  static T value;
};

using MyTypes =
  ::testing::Types<const char*, int, int*, std::string>;
TYPED_TEST_SUITE(SignalsArgs, MyTypes);

template<>
const char* SignalsArgs<const char*>::value = "data";
template<>
int SignalsArgs<int>::value = 123;
static int _intvalue = 456;
template<>
int* SignalsArgs<int*>::value = &_intvalue;
template<>
std::string SignalsArgs<std::string>::value = "data";

TEST(Signals, doubleRegister)
{
  Sender s;

  s.registerSignal("signal");
  EXPECT_THROW({
    s.registerSignal("signal");
  }, std::logic_error);
}

TEST(Signals, doubleRegisterArg)
{
  Sender s;

  s.registerSignal<const char*>("signal");
  EXPECT_THROW({
    // Doesn't matter that the type differs
    s.registerSignal<int>("signal");
  }, std::logic_error);
}

TEST(Signals, emitUnknown)
{
  Sender s;

  EXPECT_THROW({
    s.emitSignal("nosignal");
  }, std::logic_error);

  EXPECT_THROW({
    s.emitSignal("nosignal", "data");
  }, std::logic_error);
}

TEST(Signals, emitUnexpectedData)
{
  Sender s;

  s.registerSignal("signal");
  EXPECT_THROW({
    s.emitSignal("signal", "data");
  }, std::logic_error);
}

TEST(Signals, emitMissingData)
{
  Sender s;

  s.registerSignal<const char*>("signal");
  EXPECT_THROW({
    s.emitSignal("signal");
  }, std::logic_error);

  s.registerSignal<const char*, int>("signal2");
  EXPECT_THROW({
    s.emitSignal("signal2", "data");
  }, std::logic_error);
}

TEST(Signals, emitWrongData)
{
  Sender s;

  s.registerSignal<const char*>("signal");
  EXPECT_THROW({
    s.emitSignal("signal", 1234);
  }, std::logic_error);

  s.registerSignal<const char*, const char*>("signal2");
  EXPECT_THROW({
    s.emitSignal("signal2", "data", 1234);
  }, std::logic_error);
}

TEST(Signals, connectSignal)
{
  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.registerSignal("signal");
  s.connectSignal("signal", &r, &Receiver::simpleHandler);
  s.emitSignal("signal");
  EXPECT_EQ(callCount, 1);

  /* Generic handler */
  callCount = 0;
  s.registerSignal("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.emitSignal("gsignal");
  EXPECT_EQ(callCount, 1);

  /* Specific handler */
  callCount = 0;
  s.registerSignal("ssignal");
  s.connectSignal("ssignal", &r, &Receiver::specificHandler);
  s.emitSignal("ssignal");
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, connectSignalArg)
{
  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.registerSignal<TypeParam>("signal");
  s.connectSignal("signal", &r,
                  &Receiver::simpleTypeHandler<TypeParam>);
  s.emitSignal("signal", TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Generic handler */
  callCount = 0;
  s.registerSignal<TypeParam>("gsignal");
  s.connectSignal("gsignal", &r,
                  &Receiver::genericTypeHandler<TypeParam>);
  s.emitSignal("gsignal", TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Specific handler */
  callCount = 0;
  s.registerSignal<TypeParam>("ssignal");
  s.connectSignal("ssignal", &r,
                  &Receiver::specificTypeHandler<TypeParam>);
  s.emitSignal("ssignal", TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, connectSignalMultiArg)
{
  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.registerSignal<TypeParam, double>("signal");
  s.connectSignal("signal", &r,
                  &Receiver::simpleTypeHandler<TypeParam, double>);
  s.emitSignal("signal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);

  /* Generic handler */
  callCount = 0;
  s.registerSignal<TypeParam, double>("gsignal");
  s.connectSignal("gsignal", &r,
                  &Receiver::genericTypeHandler<TypeParam, double>);
  s.emitSignal("gsignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);

  /* Specific handler */
  callCount = 0;
  s.registerSignal<TypeParam, double>("ssignal");
  s.connectSignal("ssignal", &r,
                  &Receiver::specificTypeHandler<TypeParam, double>);
  s.emitSignal("ssignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, connectAmbiguous)
{
  Sender s;
  Receiver r;

  GTEST_SKIP() << "Currently broken";

  /* Simple handler */
  callCount = 0;
  s.registerSignal<Receiver*, const char*, TypeParam>("signal");
  s.connectSignal(
    "signal", &r,
    &Receiver::simpleTypeHandler<Receiver*, const char*, TypeParam>);
  s.emitSignal("signal", &r, "data", TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Generic handler */
  callCount = 0;
  s.registerSignal<Receiver*, const char*, TypeParam>("gsignal");
  s.connectSignal(
    "gsignal", &r,
    &Receiver::genericTypeHandler<Receiver*, const char*, TypeParam>);
  s.emitSignal("gsignal", &r, "data", TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Specific handler */
  callCount = 0;
  s.registerSignal<Receiver*, const char*, TypeParam>("ssignal");
  s.connectSignal(
    "ssignal", &r,
    &Receiver::specificTypeHandler<Receiver*, const char*, TypeParam>);
  s.emitSignal("ssignal", &r, "data", TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

static void connectSimpleLambda(Sender* s)
{
  s->connectSignal("msignal", []() { callCount++; });
}

static void connectLambdaWithCaptures(Sender* s, Receiver* r, int x)
{
  s->connectSignal("mcsignal", r,
                  [s, r, x]() { (void)s; (void)r; (void)x; callCount++; });
}

TEST(Signals, connectSignalLambda)
{
  Sender s;
  Receiver r;

  /* Simple lambda */
  callCount = 0;
  s.registerSignal("signal");
  s.connectSignal("signal", []() { callCount++; });
  s.emitSignal("signal");
  EXPECT_EQ(callCount, 1);

  /* Multiple simple lambdas */
  callCount = 0;
  s.registerSignal("msignal");
  connectSimpleLambda(&s);
  connectSimpleLambda(&s);
  s.emitSignal("msignal");
  EXPECT_EQ(callCount, 2);

  /* Lambda with captures */
  callCount = 0;
  s.registerSignal("csignal");
  s.connectSignal("csignal", &r,
                  [&s, &r]() { (void)s; (void)r; callCount++; });
  s.emitSignal("csignal");
  EXPECT_EQ(callCount, 1);

  /* Multiple lambdas with captures */
  callCount = 0;
  s.registerSignal("mcsignal");
  connectLambdaWithCaptures(&s, &r, 1);
  connectLambdaWithCaptures(&s, &r, 2);
  s.emitSignal("mcsignal");
  EXPECT_EQ(callCount, 2);
}

template<typename... Args>
static void connectSimpleLambdaArgs(Sender* s)
{
  s->connectSignal<Args...>("msignal", [](Args...) { callCount++; });
}

template<typename... Args>
static void connectLambdaArgsWithCaptures(Sender* s, Receiver* r, int x)
{
  s->connectSignal<Args...>("mcsignal", r, [s, r, x](Args...) {
    (void)s; (void)r; (void)x;
    callCount++;
  });
}

TYPED_TEST(SignalsArgs, connectSignalLambdaArgs)
{
  Sender s;
  Receiver r;

  /* Simple lambda */
  callCount = 0;
  s.registerSignal<TypeParam>("signal");
  s.connectSignal<TypeParam>("signal", [](TypeParam) { callCount++; });
  s.emitSignal("signal", TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Multiple simple lambdas */
  callCount = 0;
  s.registerSignal<TypeParam>("msignal");
  connectSimpleLambdaArgs<TypeParam>(&s);
  connectSimpleLambdaArgs<TypeParam>(&s);
  s.emitSignal("msignal", TestFixture::value);
  EXPECT_EQ(callCount, 2);

  /* Lambda with captures */
  callCount = 0;
  s.registerSignal<TypeParam>("csignal");
  s.connectSignal<TypeParam>("csignal", &r, [&s, &r](TypeParam) {
    (void)s; (void)r;
    callCount++;
  });
  s.emitSignal("csignal", TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Multiple lambdas with captures */
  callCount = 0;
  s.registerSignal<TypeParam>("mcsignal");
  connectLambdaArgsWithCaptures<TypeParam>(&s, &r, 1);
  connectLambdaArgsWithCaptures<TypeParam>(&s, &r, 2);
  s.emitSignal("mcsignal", TestFixture::value);
  EXPECT_EQ(callCount, 2);
}

TYPED_TEST(SignalsArgs, connectSignalLambdaMultiArgs)
{
  Sender s;
  Receiver r;

  /* Simple lambda */
  callCount = 0;
  s.registerSignal<TypeParam, double>("signal");
  s.connectSignal<TypeParam, double>(
    "signal", [](TypeParam, double) { callCount++; });
  s.emitSignal("signal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);

  /* Multiple simple lambdas */
  callCount = 0;
  s.registerSignal<TypeParam, double>("msignal");
  connectSimpleLambdaArgs<TypeParam, double>(&s);
  connectSimpleLambdaArgs<TypeParam, double>(&s);
  s.emitSignal("msignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 2);

  /* Lambda with captures */
  callCount = 0;
  s.registerSignal<TypeParam, double>("csignal");
  s.connectSignal<TypeParam, double>("csignal", &r,
                                     [&s, &r](TypeParam, double) {
                                       (void)s;
                                       (void)r;
                                       callCount++;
                                     });
  s.emitSignal("csignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);

  /* Multiple lambdas with captures */
  callCount = 0;
  s.registerSignal<TypeParam, double>("mcsignal");
  connectLambdaArgsWithCaptures<TypeParam, double>(&s, &r, 1);
  connectLambdaArgsWithCaptures<TypeParam, double>(&s, &r, 2);
  s.emitSignal("mcsignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 2);
}

TEST(Signals, connectUnknown)
{
  Sender s;
  Receiver r;

  EXPECT_THROW({
    s.connectSignal("nosignal", &r, &Receiver::genericHandler);
  }, std::logic_error);
}

TYPED_TEST(SignalsArgs, connectUnknownArg)
{
  Sender s;
  Receiver r;

  EXPECT_THROW({
    s.connectSignal("nosignal", &r,
                    &Receiver::genericTypeHandler<TypeParam>);
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

TYPED_TEST(SignalsArgs, doubleConnectArg)
{
  Sender s;
  Receiver r;

  s.registerSignal<TypeParam>("dblsignal");
  s.connectSignal("dblsignal", &r,
                  &Receiver::genericTypeHandler<TypeParam>);
  EXPECT_THROW({
    s.connectSignal("dblsignal", &r,
                    &Receiver::genericTypeHandler<TypeParam>);
  }, std::logic_error);
}

TYPED_TEST(SignalsArgs, doubleConnectMultiArg)
{
  Sender s;
  Receiver r;

  s.registerSignal<TypeParam, double>("dblsignal");
  s.connectSignal("dblsignal", &r,
                  &Receiver::genericTypeHandler<TypeParam, double>);
  EXPECT_THROW({
    s.connectSignal("dblsignal", &r,
                    &Receiver::genericTypeHandler<TypeParam, double>);
  }, std::logic_error);
}

TEST(Signals, connectUnexpectedData)
{
  Sender s;
  Receiver r;

  s.registerSignal("nodata");
  EXPECT_THROW({
    s.connectSignal("nodata", &r,
                    &Receiver::genericTypeHandler<const char*>);
  }, std::logic_error);
}

TEST(Signals, connectMissingData)
{
  Sender s;
  Receiver r;

  s.registerSignal<int>("withdata");
  EXPECT_THROW({
    s.connectSignal("withdata", &r, &Receiver::genericHandler);
  }, std::logic_error);

  s.registerSignal<const char*, int>("withpartialdata");
  EXPECT_THROW({
    s.connectSignal("withpartialdata", &r,
                    &Receiver::genericTypeHandler<const char*>);
  }, std::logic_error);
}

TEST(Signals, connectWrongData)
{
  Sender s;
  Receiver r;

  s.registerSignal<int>("withdata");
  EXPECT_THROW({
    s.connectSignal("withdata", &r,
                    &Receiver::genericTypeHandler<const char*>);
  }, std::logic_error);

  s.registerSignal<const char*, int>("withpartialdata");
  EXPECT_THROW({
    s.connectSignal("withpartialdata", &r,
                    &Receiver::genericTypeHandler<const char*, double>);
  }, std::logic_error);
}

TEST(Signals, connectBadReceiver)
{
  Sender s;
  Receiver r;

  s.registerSignal("badsignal");
  EXPECT_THROW({
    s.connectSignal("badsignal", &r, &Receiver::badSpecificHandler);
  }, std::logic_error);
}

TYPED_TEST(SignalsArgs, connectBadReceiverArgs)
{
  Sender s;
  Receiver r;

  s.registerSignal<TypeParam>("badsignal");
  EXPECT_THROW({
    s.connectSignal("badsignal", &r,
                    &Receiver::badSpecificTypeHandler<TypeParam>);
  }, std::logic_error);
}

TEST(Signals, disconnectSignal)
{
  Sender s;
  Receiver r;
  core::Connection c;

  /* Simple handler */
  callCount = 0;
  s.registerSignal("signal");
  c = s.connectSignal("signal", &r, &Receiver::simpleHandler);
  s.disconnectSignal(c);
  s.emitSignal("signal");
  EXPECT_EQ(callCount, 0);

  /* Generic handler */
  callCount = 0;
  s.registerSignal("gsignal");
  c = s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal(c);
  s.emitSignal("gsignal");
  EXPECT_EQ(callCount, 0);

  /* Specific handler */
  callCount = 0;
  s.registerSignal("ssignal");
  c = s.connectSignal("ssignal", &r, &Receiver::specificHandler);
  s.disconnectSignal(c);
  s.emitSignal("ssignal");
  EXPECT_EQ(callCount, 0);

  /* Simple lambda */
  callCount = 0;
  s.registerSignal("lsignal");
  c = s.connectSignal("lsignal", []() { callCount++; });
  s.disconnectSignal(c);
  s.emitSignal("lsignal");
  EXPECT_EQ(callCount, 0);

  /* Lambda with captures */
  callCount = 0;
  s.registerSignal("lcsignal");
  c = s.connectSignal("lcsignal", &r,
                      [&s, &r]() { (void)s; (void)r; callCount++; });
  s.disconnectSignal(c);
  s.emitSignal("lcsignal");
  EXPECT_EQ(callCount, 0);
}

TYPED_TEST(SignalsArgs, disconnectSignalArg)
{
  Sender s;
  Receiver r;
  core::Connection c;

  /* Simple handler with args */
  callCount = 0;
  s.registerSignal<TypeParam>("signal");
  c = s.connectSignal("signal", &r,
                      &Receiver::simpleTypeHandler<TypeParam>);
  s.disconnectSignal(c);
  s.emitSignal("signal", TestFixture::value);
  EXPECT_EQ(callCount, 0);

  /* Generic handler with args */
  callCount = 0;
  s.registerSignal<TypeParam>("gsignal");
  c = s.connectSignal("gsignal", &r,
                      &Receiver::genericTypeHandler<TypeParam>);
  s.disconnectSignal(c);
  s.emitSignal("gsignal", TestFixture::value);
  EXPECT_EQ(callCount, 0);

  /* Specific handler with args */
  callCount = 0;
  s.registerSignal<TypeParam>("ssignal");
  c = s.connectSignal("ssignal", &r,
                      &Receiver::specificTypeHandler<TypeParam>);
  s.disconnectSignal(c);
  s.emitSignal("ssignal", TestFixture::value);
  EXPECT_EQ(callCount, 0);

  /* Simple lambda with args */
  callCount = 0;
  s.registerSignal<TypeParam>("lsignal");
  c = s.connectSignal<TypeParam>("lsignal",
                                 [](TypeParam) { callCount++; });
  s.disconnectSignal(c);
  s.emitSignal("lsignal", TestFixture::value);
  EXPECT_EQ(callCount, 0);

  /* Lambda with captures and args */
  callCount = 0;
  s.registerSignal<TypeParam>("lcsignal");
  c = s.connectSignal<TypeParam>("lcsignal", &r,
                                 [&s, &r](TypeParam) {
                                   (void)s; (void)r;
                                   callCount++;
                                 });
  s.disconnectSignal(c);
  s.emitSignal("lcsignal", TestFixture::value);
  EXPECT_EQ(callCount, 0);
}

TYPED_TEST(SignalsArgs, disconnectSignalMultiArg)
{
  Sender s;
  Receiver r;
  core::Connection c;

  /* Simple handler with args */
  callCount = 0;
  s.registerSignal<TypeParam, double>("signal");
  c = s.connectSignal("signal", &r,
                      &Receiver::simpleTypeHandler<TypeParam, double>);
  s.disconnectSignal(c);
  s.emitSignal("signal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);

  /* Generic handler with args */
  callCount = 0;
  s.registerSignal<TypeParam, double>("gsignal");
  c = s.connectSignal("gsignal", &r,
                      &Receiver::genericTypeHandler<TypeParam, double>);
  s.disconnectSignal(c);
  s.emitSignal("gsignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);

  /* Specific handler with args */
  callCount = 0;
  s.registerSignal<TypeParam, double>("ssignal");
  c = s.connectSignal("ssignal", &r,
                      &Receiver::specificTypeHandler<TypeParam, double>);
  s.disconnectSignal(c);
  s.emitSignal("ssignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);

  /* Simple lambda with args */
  callCount = 0;
  s.registerSignal<TypeParam, double>("lsignal");
  c = s.connectSignal<TypeParam, double>("lsignal",
                                 [](TypeParam, double) { callCount++; });
  s.disconnectSignal(c);
  s.emitSignal("lsignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);

  /* Lambda with captures and args */
  callCount = 0;
  s.registerSignal<TypeParam, double>("lcsignal");
  c = s.connectSignal<TypeParam, double>("lcsignal", &r,
                                         [&s, &r](TypeParam, double) {
                                           (void)s;
                                           (void)r;
                                           callCount++;
                                         });
  s.disconnectSignal(c);
  s.emitSignal("lcsignal", TestFixture::value, 1.2);
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

  /* Simple handler */
  callCount = 0;
  s.registerSignal("signal");
  s.connectSignal("signal", &r, &Receiver::simpleHandler);
  s.disconnectSignal("signal", &r, &Receiver::simpleHandler);
  s.emitSignal("signal");
  EXPECT_EQ(callCount, 0);

  /* Generic handler */
  callCount = 0;
  s.registerSignal("gsignal");
  s.connectSignal("gsignal", &r, &Receiver::genericHandler);
  s.disconnectSignal("gsignal", &r, &Receiver::genericHandler);
  s.emitSignal("gsignal");
  EXPECT_EQ(callCount, 0);

  /* Specific handler */
  callCount = 0;
  s.registerSignal("ssignal");
  s.connectSignal("ssignal", &r, &Receiver::specificHandler);
  s.disconnectSignal("ssignal", &r, &Receiver::specificHandler);
  s.emitSignal("ssignal");
  EXPECT_EQ(callCount, 0);
}

TYPED_TEST(SignalsArgs, disconnectHelperArg)
{
  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.registerSignal<TypeParam>("signal");
  s.connectSignal("signal", &r,
                  &Receiver::simpleTypeHandler<TypeParam>);
  s.disconnectSignal("signal", &r,
                     &Receiver::simpleTypeHandler<TypeParam>);
  s.emitSignal("signal", TestFixture::value);
  EXPECT_EQ(callCount, 0);

  /* Generic handler */
  callCount = 0;
  s.registerSignal<TypeParam>("gsignal");
  s.connectSignal("gsignal", &r,
                  &Receiver::genericTypeHandler<TypeParam>);
  s.disconnectSignal("gsignal", &r,
                     &Receiver::genericTypeHandler<TypeParam>);
  s.emitSignal("gsignal", TestFixture::value);
  EXPECT_EQ(callCount, 0);

  /* Specific handler */
  callCount = 0;
  s.registerSignal<TypeParam>("ssignal");
  s.connectSignal("ssignal", &r,
                  &Receiver::specificTypeHandler<TypeParam>);
  s.disconnectSignal("ssignal", &r,
                     &Receiver::specificTypeHandler<TypeParam>);
  s.emitSignal("ssignal", TestFixture::value);
  EXPECT_EQ(callCount, 0);
}

TYPED_TEST(SignalsArgs, disconnectHelperMultiArg)
{
  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.registerSignal<TypeParam, double>("signal");
  s.connectSignal("signal", &r,
                  &Receiver::simpleTypeHandler<TypeParam, double>);
  s.disconnectSignal("signal", &r,
                     &Receiver::simpleTypeHandler<TypeParam, double>);
  s.emitSignal("signal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);

  /* Generic handler */
  callCount = 0;
  s.registerSignal<TypeParam, double>("gsignal");
  s.connectSignal("gsignal", &r,
                  &Receiver::genericTypeHandler<TypeParam, double>);
  s.disconnectSignal("gsignal", &r,
                     &Receiver::genericTypeHandler<TypeParam, double>);
  s.emitSignal("gsignal", TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);

  /* Specific handler */
  callCount = 0;
  s.registerSignal<TypeParam, double>("ssignal");
  s.connectSignal("ssignal", &r,
                  &Receiver::specificTypeHandler<TypeParam, double>);
  s.disconnectSignal("ssignal", &r,
                     &Receiver::specificTypeHandler<TypeParam, double>);
  s.emitSignal("ssignal", TestFixture::value, 1.2);
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

TYPED_TEST(SignalsArgs, disconnectSimilarArg)
{
  Sender s;
  Receiver r;

  callCount = 0;
  s.registerSignal<TypeParam>("osignal");
  s.connectSignal("osignal", &r,
                  &Receiver::genericTypeHandler<TypeParam>);
  s.connectSignal("osignal", &r,
                  &Receiver::otherGenericTypeHandler<TypeParam>);
  s.disconnectSignal("osignal", &r,
                     &Receiver::genericTypeHandler<TypeParam>);
  s.emitSignal("osignal", TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, disconnectSimilarMultiArg)
{
  Sender s;
  Receiver r;

  callCount = 0;
  s.registerSignal<TypeParam, double>("osignal");
  s.connectSignal("osignal", &r,
                  &Receiver::genericTypeHandler<TypeParam, double>);
  s.connectSignal("osignal", &r,
                  &Receiver::otherGenericTypeHandler<TypeParam, double>);
  s.disconnectSignal("osignal", &r,
                     &Receiver::genericTypeHandler<TypeParam, double>);
  s.emitSignal("osignal", TestFixture::value, 1.2);
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

TYPED_TEST(SignalsArgs, disconnectUnknownArg)
{
  Sender s;
  Receiver r;

  EXPECT_THROW({
    s.disconnectSignal("nosignal", &r,
                       &Receiver::genericTypeHandler<TypeParam>);
  }, std::logic_error);
}

TEST(Signals, disconnectAll)
{
  Sender s;
  Receiver r;
  Receiver r2;

  callCount = 0;
  s.registerSignal("signal1");
  s.registerSignal("signal2");
  s.registerSignal<const char*>("signal3");
  s.registerSignal<const char*, double>("signal4");
  s.connectSignal("signal1", &r, &Receiver::simpleHandler);
  s.connectSignal("signal1", &r, &Receiver::genericHandler);
  s.connectSignal("signal1", &r, [] { callCount++; });
  s.connectSignal("signal2", &r, &Receiver::genericHandler);
  s.connectSignal("signal3", &r,
                  &Receiver::simpleTypeHandler<const char*>);
  s.connectSignal("signal3", &r,
                  &Receiver::genericTypeHandler<const char*>);
  s.connectSignal<const char*>("signal3", &r,
                               [](const char*) { callCount++; });
  s.connectSignal("signal4", &r,
                  &Receiver::specificTypeHandler<const char*, double>);
  s.connectSignal("signal1", &r2, &Receiver::genericHandler);
  s.disconnectSignals(&r);
  s.emitSignal("signal1");
  s.emitSignal("signal2");
  s.emitSignal("signal3", "data");
  s.emitSignal("signal4", "data", 1.2);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, implicitDisconnect)
{
  Sender s;

  callCount = 0;
  s.registerSignal("isignal");
  {
    Receiver scoped_r;
    s.connectSignal("isignal", &scoped_r, &Receiver::genericHandler);
  }
  s.emitSignal("isignal");
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, addWhileEmitting)
{
  Sender s;
  Receiver r;

  callCount = 0;
  s.registerSignal("asignal");
  s.connectSignal("asignal", &r, &Receiver::registerHandler);
  s.connectSignal("asignal", &r, &Receiver::otherGenericHandler);
  s.emitSignal("asignal");
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, removeWhileEmitting)
{
  Sender s;
  Receiver r;

  callCount = 0;
  s.registerSignal("rsignal");
  s.connectSignal("rsignal", &r, &Receiver::unregisterHandler);
  s.connectSignal("rsignal", &r, &Receiver::genericHandler);
  s.connectSignal("rsignal", &r, &Receiver::otherGenericHandler);
  s.emitSignal("rsignal");
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, emitRefConversion)
{
  Sender s;
  Receiver r;

  /* Receiver adds reference */
  callCount = 0;
  s.registerSignal<TypeParam>("refhandler");
  s.connectSignal("refhandler", &r,
                  &Receiver::simpleTypeHandler<const TypeParam&>);
  s.connectSignal("refhandler", &r,
                  &Receiver::genericTypeHandler<const TypeParam&>);
  s.connectSignal<const TypeParam&>(
    "refhandler", [](const TypeParam&) { callCount++; });
  s.connectSignal<const TypeParam&>(
    "refhandler", &r, [&r](const TypeParam&) { (void)r; callCount++; });
  s.emitSignal("refhandler", TestFixture::value);
  EXPECT_EQ(callCount, 4);

  /* Sender adds reference */
  callCount = 0;
  s.registerSignal<const TypeParam&>("refemitter");
  s.connectSignal("refemitter", &r,
                  &Receiver::simpleTypeHandler<const TypeParam&>);
  s.connectSignal("refemitter", &r,
                  &Receiver::genericTypeHandler<const TypeParam&>);
  s.connectSignal<const TypeParam&>(
    "refemitter", [](const TypeParam&) { callCount++; });
  s.connectSignal<const TypeParam&>(
    "refemitter", &r, [&r](const TypeParam&) { (void)r; callCount++; });
  s.emitSignal("refemitter", TestFixture::value);
  EXPECT_EQ(callCount, 4);
}

TYPED_TEST(SignalsArgs, emitConstConversion)
{
  Sender s;
  Receiver r;

  TypeParam value = TestFixture::value;

  GTEST_SKIP() << "Currently broken";

  /* Receiver adds pointer const qualifier */
  callCount = 0;
  s.registerSignal<TypeParam*>("consthandler");
  s.connectSignal("consthandler", &r,
                  &Receiver::simpleTypeHandler<const TypeParam*>);
  s.connectSignal("consthandler", &r,
                  &Receiver::genericTypeHandler<const TypeParam*>);
  s.connectSignal<const TypeParam*>(
    "consthandler", [](const TypeParam*) { callCount++; });
  s.connectSignal<const TypeParam*>(
    "consthandler", &r, [&r](const TypeParam*) { (void)r; callCount++; });
  s.emitSignal("consthandler", &value);
  EXPECT_EQ(callCount, 4);

  /* Sender adds pointer const qualifier */
  callCount = 0;
  s.registerSignal<const TypeParam*>("constemitter");
  s.connectSignal("constemitter", &r,
                  &Receiver::simpleTypeHandler<const TypeParam*>);
  s.connectSignal("constemitter", &r,
                  &Receiver::genericTypeHandler<const TypeParam*>);
  s.connectSignal<const TypeParam*>(
    "constemitter", [](const TypeParam*) { callCount++; });
  s.connectSignal<const TypeParam*>(
    "constemitter", &r, [&r](const TypeParam*) { (void)r; callCount++; });
  s.emitSignal("constemitter", &value);
  EXPECT_EQ(callCount, 4);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
