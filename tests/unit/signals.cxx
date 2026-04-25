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

class SenderBase : public core::Object {
public:
  SenderBase() {}

  template<typename... SigArgs, typename... Args>
  void emitSignal(const core::signal<SigArgs...>* sig, Args... args)
  {
    core::Object::emitSignal(sig, args...);
  }
};

class Receiver : public core::Object {
public:
  Receiver() {}

  void simpleHandler() { callCount++; }
  void otherSimpleHandler() { callCount++; }

  template<typename... Args>
  void simpleTypeHandler(Args...) { callCount++; }
  template<typename... Args>
  void otherSimpleTypeHandler(Args...) { callCount++; }
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

TEST(Signals, connectSignal)
{
  class Sender : public SenderBase {
  public:
    core::signal<> signal;
  };

  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.connectSignal(&s.signal, &r, &Receiver::simpleHandler);
  s.emitSignal(&s.signal);
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, connectSignalArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> signal;
  };

  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.connectSignal(&s.signal, &r,
                  &Receiver::simpleTypeHandler<TypeParam>);
  s.emitSignal(&s.signal, TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, connectSignalMultiArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam, double> signal;
  };

  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.connectSignal(&s.signal, &r,
                  &Receiver::simpleTypeHandler<TypeParam, double>);
  s.emitSignal(&s.signal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, connectAmbiguous)
{
  class Sender : public SenderBase {
  public:
    core::signal<Receiver*, TypeParam> signal;
  };

  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.connectSignal(&s.signal, &r,
                  &Receiver::simpleTypeHandler<Receiver*, TypeParam>);
  s.emitSignal(&s.signal, &r, TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

static void connectSimpleLambda(SenderBase* s, core::signal<>* signal)
{
  s->connectSignal(signal, []() { callCount++; });
}

static void connectLambdaWithCaptures(SenderBase* s,
                                      core::signal<>* signal,
                                      Receiver* r, int x)
{
  s->connectSignal(signal, r,
                  [s, r, x]() { (void)s; (void)r; (void)x; callCount++; });
}

TEST(Signals, connectSignalLambda)
{
  class Sender : public SenderBase {
  public:
    core::signal<> signal, msignal, csignal, mcsignal;
  };

  Sender s;
  Receiver r;

  /* Simple lambda */
  callCount = 0;
  s.connectSignal(&s.signal, []() { callCount++; });
  s.emitSignal(&s.signal);
  EXPECT_EQ(callCount, 1);

  /* Multiple simple lambdas */
  callCount = 0;
  connectSimpleLambda(&s, &s.msignal);
  connectSimpleLambda(&s, &s.msignal);
  s.emitSignal(&s.msignal);
  EXPECT_EQ(callCount, 2);

  /* Lambda with captures */
  callCount = 0;
  s.connectSignal(&s.csignal, &r,
                  [&s, &r]() { (void)s; (void)r; callCount++; });
  s.emitSignal(&s.csignal);
  EXPECT_EQ(callCount, 1);

  /* Multiple lambdas with captures */
  callCount = 0;
  connectLambdaWithCaptures(&s, &s.mcsignal, &r, 1);
  connectLambdaWithCaptures(&s, &s.mcsignal, &r, 2);
  s.emitSignal(&s.mcsignal);
  EXPECT_EQ(callCount, 2);
}

template<typename... Args>
static void connectSimpleLambdaArgs(SenderBase* s,
                                    core::signal<Args...>* signal)
{
  s->connectSignal(signal, [](Args...) { callCount++; });
}

template<typename... Args>
static void connectLambdaArgsWithCaptures(SenderBase* s,
                                          core::signal<Args...>* signal,
                                          Receiver* r, int x)
{
  s->connectSignal(signal, r, [s, r, x](Args...) {
    (void)s; (void)r; (void)x;
    callCount++;
  });
}

TYPED_TEST(SignalsArgs, connectSignalLambdaArgs)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> signal, msignal, csignal, mcsignal;
  };

  Sender s;
  Receiver r;

  /* Simple lambda */
  callCount = 0;
  s.connectSignal(&s.signal, [](TypeParam) { callCount++; });
  s.emitSignal(&s.signal, TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Multiple simple lambdas */
  callCount = 0;
  connectSimpleLambdaArgs(&s, &s.msignal);
  connectSimpleLambdaArgs(&s, &s.msignal);
  s.emitSignal(&s.msignal, TestFixture::value);
  EXPECT_EQ(callCount, 2);

  /* Lambda with captures */
  callCount = 0;
  s.connectSignal(&s.csignal, &r, [&s, &r](TypeParam) {
    (void)s; (void)r;
    callCount++;
  });
  s.emitSignal(&s.csignal, TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Multiple lambdas with captures */
  callCount = 0;
  connectLambdaArgsWithCaptures(&s, &s.mcsignal, &r, 1);
  connectLambdaArgsWithCaptures(&s, &s.mcsignal, &r, 2);
  s.emitSignal(&s.mcsignal, TestFixture::value);
  EXPECT_EQ(callCount, 2);
}

TYPED_TEST(SignalsArgs, connectSignalLambdaMultiArgs)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam, double> signal, msignal, csignal, mcsignal;
  };

  Sender s;
  Receiver r;

  /* Simple lambda */
  callCount = 0;
  s.connectSignal(&s.signal, [](TypeParam, double) { callCount++; });
  s.emitSignal(&s.signal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);

  /* Multiple simple lambdas */
  callCount = 0;
  connectSimpleLambdaArgs(&s, &s.msignal);
  connectSimpleLambdaArgs(&s, &s.msignal);
  s.emitSignal(&s.msignal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 2);

  /* Lambda with captures */
  callCount = 0;
  s.connectSignal(&s.csignal, &r, [&s, &r](TypeParam, double) {
    (void)s;
    (void)r;
    callCount++;
  });
  s.emitSignal(&s.csignal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);

  /* Multiple lambdas with captures */
  callCount = 0;
  connectLambdaArgsWithCaptures(&s, &s.mcsignal, &r, 1);
  connectLambdaArgsWithCaptures(&s, &s.mcsignal, &r, 2);
  s.emitSignal(&s.mcsignal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 2);
}

TEST(Signals, doubleConnect)
{
  class Sender : public SenderBase {
  public:
    core::signal<> dblsignal;
  };

  Sender s;
  Receiver r;

  s.connectSignal(&s.dblsignal, &r, &Receiver::simpleHandler);
  EXPECT_THROW({
    s.connectSignal(&s.dblsignal, &r, &Receiver::simpleHandler);
  }, std::logic_error);
}

TYPED_TEST(SignalsArgs, doubleConnectArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> dblsignal;
  };

  Sender s;
  Receiver r;

  s.connectSignal(&s.dblsignal, &r,
                  &Receiver::simpleTypeHandler<TypeParam>);
  EXPECT_THROW({
    s.connectSignal(&s.dblsignal, &r,
                    &Receiver::simpleTypeHandler<TypeParam>);
  }, std::logic_error);
}

TYPED_TEST(SignalsArgs, doubleConnectMultiArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam, double> dblsignal;
  };

  Sender s;
  Receiver r;

  s.connectSignal(&s.dblsignal, &r,
                  &Receiver::simpleTypeHandler<TypeParam, double>);
  EXPECT_THROW({
    s.connectSignal(&s.dblsignal, &r,
                    &Receiver::simpleTypeHandler<TypeParam, double>);
  }, std::logic_error);
}

TEST(Signals, disconnectSignal)
{
  class Sender : public SenderBase {
  public:
    core::signal<> signal, lsignal, lcsignal;
  };

  Sender s;
  Receiver r;
  core::Connection c;

  /* Simple handler */
  callCount = 0;
  c = s.connectSignal(&s.signal, &r, &Receiver::simpleHandler);
  s.disconnectSignal(c);
  s.emitSignal(&s.signal);
  EXPECT_EQ(callCount, 0);

  /* Simple lambda */
  callCount = 0;
  c = s.connectSignal(&s.lsignal, []() { callCount++; });
  s.disconnectSignal(c);
  s.emitSignal(&s.lsignal);
  EXPECT_EQ(callCount, 0);

  /* Lambda with captures */
  callCount = 0;
  c = s.connectSignal(&s.lcsignal, &r,
                      [&s, &r]() { (void)s; (void)r; callCount++; });
  s.disconnectSignal(c);
  s.emitSignal(&s.lcsignal);
  EXPECT_EQ(callCount, 0);
}

TYPED_TEST(SignalsArgs, disconnectSignalArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> signal, lsignal, lcsignal;
  };

  Sender s;
  Receiver r;
  core::Connection c;

  /* Simple handler with args */
  callCount = 0;
  c = s.connectSignal(&s.signal, &r,
                      &Receiver::simpleTypeHandler<TypeParam>);
  s.disconnectSignal(c);
  s.emitSignal(&s.signal, TestFixture::value);
  EXPECT_EQ(callCount, 0);

  /* Simple lambda with args */
  callCount = 0;
  c = s.connectSignal(&s.lsignal, [](TypeParam) { callCount++; });
  s.disconnectSignal(c);
  s.emitSignal(&s.lsignal, TestFixture::value);
  EXPECT_EQ(callCount, 0);

  /* Lambda with captures and args */
  callCount = 0;
  c = s.connectSignal(&s.lcsignal, &r, [&s, &r](TypeParam) {
    (void)s; (void)r; callCount++;
});
  s.disconnectSignal(c);
  s.emitSignal(&s.lcsignal, TestFixture::value);
  EXPECT_EQ(callCount, 0);
}

TYPED_TEST(SignalsArgs, disconnectSignalMultiArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam, double> signal, lsignal, lcsignal;
  };

  Sender s;
  Receiver r;
  core::Connection c;

  /* Simple handler with args */
  callCount = 0;
  c = s.connectSignal(&s.signal, &r,
                      &Receiver::simpleTypeHandler<TypeParam, double>);
  s.disconnectSignal(c);
  s.emitSignal(&s.signal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);

  /* Simple lambda with args */
  callCount = 0;
  c = s.connectSignal(&s.lsignal, [](TypeParam, double) { callCount++; });
  s.disconnectSignal(c);
  s.emitSignal(&s.lsignal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);

  /* Lambda with captures and args */
  callCount = 0;
  c = s.connectSignal(&s.lcsignal, &r, [&s, &r](TypeParam, double) {
    (void)s; (void)r; callCount++;
  });
  s.disconnectSignal(c);
  s.emitSignal(&s.lcsignal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, doubleDisconnect)
{
  class Sender : public SenderBase {
  public:
    core::signal<> dblsignal;
  };

  Sender s;
  Receiver r;
  core::Connection c;

  c = s.connectSignal(&s.dblsignal, &r, &Receiver::simpleHandler);
  s.disconnectSignal(c);
  EXPECT_NO_THROW({
    s.disconnectSignal(c);
  });
}

TEST(Signals, disconnectWrongObject)
{
  class Sender : public SenderBase {
  public:
    core::signal<> othersignal;
  };

  Sender s;
  Sender s2;
  Receiver r;
  core::Connection c;

  c = s.connectSignal(&s.othersignal, &r, &Receiver::simpleHandler);
  EXPECT_THROW({
    s2.disconnectSignal(c);
  }, std::logic_error);
}

TEST(Signals, disconnectHelper)
{
  class Sender : public SenderBase {
  public:
    core::signal<> signal;
  };

  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.connectSignal(&s.signal, &r, &Receiver::simpleHandler);
  s.disconnectSignal(&s.signal, &r, &Receiver::simpleHandler);
  s.emitSignal(&s.signal);
  EXPECT_EQ(callCount, 0);
}

TYPED_TEST(SignalsArgs, disconnectHelperArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> signal;
  };

  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.connectSignal(&s.signal, &r,
                  &Receiver::simpleTypeHandler<TypeParam>);
  s.disconnectSignal(&s.signal, &r,
                     &Receiver::simpleTypeHandler<TypeParam>);
  s.emitSignal(&s.signal, TestFixture::value);
  EXPECT_EQ(callCount, 0);
}

TYPED_TEST(SignalsArgs, disconnectHelperMultiArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam, double> signal;
  };

  Sender s;
  Receiver r;

  /* Simple handler */
  callCount = 0;
  s.connectSignal(&s.signal, &r,
                  &Receiver::simpleTypeHandler<TypeParam, double>);
  s.disconnectSignal(&s.signal, &r,
                     &Receiver::simpleTypeHandler<TypeParam, double>);
  s.emitSignal(&s.signal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, disconnectSimilar)
{
  class Sender : public SenderBase {
  public:
    core::signal<> osignal;
  };

  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(&s.osignal, &r, &Receiver::simpleHandler);
  s.connectSignal(&s.osignal, &r, &Receiver::otherSimpleHandler);
  s.disconnectSignal(&s.osignal, &r, &Receiver::simpleHandler);
  s.emitSignal(&s.osignal);
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, disconnectSimilarArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> osignal;
  };

  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(&s.osignal, &r,
                  &Receiver::simpleTypeHandler<TypeParam>);
  s.connectSignal(&s.osignal, &r,
                  &Receiver::otherSimpleTypeHandler<TypeParam>);
  s.disconnectSignal(&s.osignal, &r,
                     &Receiver::simpleTypeHandler<TypeParam>);
  s.emitSignal(&s.osignal, TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

TYPED_TEST(SignalsArgs, disconnectSimilarMultiArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam, double> osignal;
  };

  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(&s.osignal, &r,
                  &Receiver::simpleTypeHandler<TypeParam, double>);
  s.connectSignal(&s.osignal, &r,
                  &Receiver::otherSimpleTypeHandler<TypeParam, double>);
  s.disconnectSignal(&s.osignal, &r,
                     &Receiver::simpleTypeHandler<TypeParam, double>);
  s.emitSignal(&s.osignal, TestFixture::value, 1.2);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, disconnectAll)
{
  class Sender : public SenderBase {
  public:
    core::signal<> signal1, signal2;
    core::signal<const char*> signal3;
    core::signal<const char*, double> signal4;
  };

  Sender s;
  Receiver r;
  Receiver r2;

  callCount = 0;
  s.connectSignal(&s.signal1, &r, &Receiver::simpleHandler);
  s.connectSignal(&s.signal1, &r, [] { callCount++; });
  s.connectSignal(&s.signal2, &r, &Receiver::simpleHandler);
  s.connectSignal(&s.signal3, &r,
                  &Receiver::simpleTypeHandler<const char*>);
  s.connectSignal(&s.signal3, &r, [](const char*) { callCount++; });
  s.connectSignal(&s.signal4, &r,
                  &Receiver::simpleTypeHandler<const char*, double>);
  s.connectSignal(&s.signal1, &r2, &Receiver::simpleHandler);
  s.disconnectSignals(&r);
  s.emitSignal(&s.signal1);
  s.emitSignal(&s.signal2);
  s.emitSignal(&s.signal3, "data");
  s.emitSignal(&s.signal4, "data", 1.2);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, implicitDisconnect)
{
  class Sender : public SenderBase {
  public:
    core::signal<> isignal;
  };

  Sender s;

  callCount = 0;
  {
    Receiver scoped_r;
    s.connectSignal(&s.isignal, &scoped_r, &Receiver::simpleHandler);
  }
  s.emitSignal(&s.isignal);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, addWhileEmitting)
{
  class Sender : public SenderBase {
  public:
    core::signal<> asignal;
  };

  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(&s.asignal, &r, [&s, &r]() {
    s.connectSignal(&s.asignal, &r, &Receiver::simpleHandler);
  });
  s.connectSignal(&s.asignal, &r, &Receiver::otherSimpleHandler);
  s.emitSignal(&s.asignal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, removeWhileEmitting)
{
  class Sender : public SenderBase {
  public:
    core::signal<> rsignal;
  };

  Sender s;
  Receiver r;

  callCount = 0;
  s.connectSignal(&s.rsignal, &r, [&s, &r]() {
    s.disconnectSignal(&s.rsignal, &r, &Receiver::simpleHandler);
  });
  s.connectSignal(&s.rsignal, &r, &Receiver::simpleHandler);
  s.connectSignal(&s.rsignal, &r, &Receiver::otherSimpleHandler);
  s.emitSignal(&s.rsignal);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, emitIntConversion)
{
  class Sender : public SenderBase {
  public:
    core::signal<double> inthandler;
    core::signal<int> intemitter;
  };

  Sender s;
  Receiver r;

  /* Receiver casts to int */
  callCount = 0;
  s.connectSignal(&s.inthandler, &r,
                  &Receiver::simpleTypeHandler<int>);
  s.connectSignal(&s.inthandler, [](int) { callCount++; });
  s.connectSignal(&s.inthandler, &r,
                  [&r](int) { (void)r; callCount++; });
  s.emitSignal(&s.inthandler, 1.2);
  EXPECT_EQ(callCount, 3);

  /* Sender casts to int */
  callCount = 0;
  s.connectSignal(&s.intemitter, &r,
                  &Receiver::simpleTypeHandler<int>);
  s.connectSignal(&s.intemitter, [](int) { callCount++; });
  s.connectSignal(&s.intemitter, &r,
                  [&r](int) { (void)r; callCount++; });
  s.emitSignal(&s.intemitter, 1.2);
  s.emitSignal(&s.intemitter, (unsigned long long)123);
  EXPECT_EQ(callCount, 6);
}

TYPED_TEST(SignalsArgs, emitRefConversion)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> refhandler;
    core::signal<const TypeParam&> refemitter;
  };

  Sender s;
  Receiver r;

  /* Receiver adds reference */
  callCount = 0;
  s.connectSignal(&s.refhandler, &r,
                  &Receiver::simpleTypeHandler<const TypeParam&>);
  s.connectSignal(&s.refhandler, [](const TypeParam&) { callCount++; });
  s.connectSignal(&s.refhandler, &r,
                  [&r](const TypeParam&) { (void)r; callCount++; });
  s.emitSignal(&s.refhandler, TestFixture::value);
  EXPECT_EQ(callCount, 3);

  /* Sender adds reference */
  callCount = 0;
  s.connectSignal(&s.refemitter, &r,
                  &Receiver::simpleTypeHandler<const TypeParam&>);
  s.connectSignal(&s.refemitter, [](const TypeParam&) { callCount++; });
  s.connectSignal(&s.refemitter, &r,
                  [&r](const TypeParam&) { (void)r; callCount++; });
  s.emitSignal(&s.refemitter, TestFixture::value);
  EXPECT_EQ(callCount, 3);
}

TYPED_TEST(SignalsArgs, emitConstConversion)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam*> consthandler;
    core::signal<const TypeParam*> constemitter;
  };

  Sender s;
  Receiver r;

  TypeParam value = TestFixture::value;

  /* Receiver adds pointer const qualifier */
  callCount = 0;
  s.connectSignal(&s.consthandler, &r,
                  &Receiver::simpleTypeHandler<const TypeParam*>);
  s.connectSignal(&s.consthandler, [](const TypeParam*) { callCount++; });
  s.connectSignal(&s.consthandler, &r,
                  [&r](const TypeParam*) { (void)r; callCount++; });
  s.emitSignal(&s.consthandler, &value);
  EXPECT_EQ(callCount, 3);

  /* Sender adds pointer const qualifier */
  callCount = 0;
  s.connectSignal(&s.constemitter, &r,
                  &Receiver::simpleTypeHandler<const TypeParam*>);
  s.connectSignal(&s.constemitter, [](const TypeParam*) { callCount++; });
  s.connectSignal(&s.constemitter, &r,
                  [&r](const TypeParam*) { (void)r; callCount++; });
  s.emitSignal(&s.constemitter, &value);
  EXPECT_EQ(callCount, 3);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
