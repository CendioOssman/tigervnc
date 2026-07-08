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
  void emitSignal(const core::signal<> S::* signal)
  {
    core::Object::emitSignal(signal);
  }
  template<class S, typename SI, typename I>
  void emitSignal(const core::signal<SI> S::* signal, I info)
  {
    core::Object::emitSignal(signal, info);
  }
};

class Receiver : public core::Object {
public:
  Receiver() {}

  void handler() { callCount++; }
  void otherHandler() { callCount++; }

  template<typename T>
  void typeHandler(T) { callCount++; }
  template<typename T>
  void otherTypeHandler(T) { callCount++; }
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

  /* Normal handler */
  callCount = 0;
  s.connectSignal(&Sender::signal, &r, &Receiver::handler);
  s.emitSignal(&Sender::signal);
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

  /* Normal handler */
  callCount = 0;
  s.connectSignal(&Sender::signal, &r,
                  &Receiver::typeHandler<TypeParam>);
  s.emitSignal(&Sender::signal, TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, connectBadSignal)
{
  class SenderA : public SenderBase {
  public:
    core::signal<> goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal<> badsignal;
  };

  SenderA s;
  Receiver r;

  s.connectSignal(&SenderA::goodsignal, &r, &Receiver::handler);
  EXPECT_THROW({
    s.connectSignal(&SenderB::badsignal, &r, &Receiver::handler);
  }, std::logic_error);
}

TYPED_TEST(SignalsArgs, connectBadSignalArg)
{
  class SenderA : public SenderBase {
  public:
    core::signal<TypeParam> goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal<TypeParam> badsignal;
  };

  SenderA s;
  Receiver r;

  s.connectSignal(&SenderA::goodsignal, &r,
                  &Receiver::typeHandler<TypeParam>);
  EXPECT_THROW({
    s.connectSignal(&SenderB::badsignal, &r,
                    &Receiver::typeHandler<TypeParam>);
  }, std::logic_error);
}

TEST(Signals, doubleConnect)
{
  class Sender : public SenderBase {
  public:
    core::signal<> dblsignal;
  };

  Sender s;
  Receiver r;

  s.connectSignal(&Sender::dblsignal, &r, &Receiver::handler);
  EXPECT_THROW({
    s.connectSignal(&Sender::dblsignal, &r, &Receiver::handler);
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

  s.connectSignal(&Sender::dblsignal, &r,
                  &Receiver::typeHandler<TypeParam>);
  EXPECT_THROW({
    s.connectSignal(&Sender::dblsignal, &r,
                    &Receiver::typeHandler<TypeParam>);
  }, std::logic_error);
}

TEST(Signals, connectSubclass)
{
  class Sender : public SenderBase {
  public:
    core::signal<> signal;
  };

  class SubReceiver : public Receiver {};

  Sender s;
  SubReceiver r;

  GTEST_SKIP() << "Currently broken";

  /* Normal handler */
  // callCount = 0;
  // s.connectSignal(&Sender::signal, &r, &Receiver::handler);
  // s.emitSignal(&Sender::signal);
  // EXPECT_EQ(callCount, 1);
}

TEST(Signals, disconnectSignal)
{
  class Sender : public SenderBase {
  public:
    core::signal<> signal;
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

TYPED_TEST(SignalsArgs, disconnectSignalArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> signal;
  };

  Sender s;
  Receiver r;
  core::Connection c;

  /* Normal handler with args */
  callCount = 0;
  c = s.connectSignal(&Sender::signal, &r,
                      &Receiver::typeHandler<TypeParam>);
  s.disconnectSignal(c);
  s.emitSignal(&Sender::signal, TestFixture::value);
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
    core::signal<> othersignal;
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
    core::signal<> signal;
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

TYPED_TEST(SignalsArgs, disconnectHelperArg)
{
  class Sender : public SenderBase {
  public:
    core::signal<TypeParam> signal;
  };

  Sender s;
  Receiver r;

  /* Normal handler */
  callCount = 0;
  s.connectSignal(&Sender::signal, &r,
                  &Receiver::typeHandler<TypeParam>);
  s.disconnectSignal(&Sender::signal, &r,
                     &Receiver::typeHandler<TypeParam>);
  s.emitSignal(&Sender::signal, TestFixture::value);
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
  s.connectSignal(&Sender::osignal, &r, &Receiver::handler);
  s.connectSignal(&Sender::osignal, &r, &Receiver::otherHandler);
  s.disconnectSignal(&Sender::osignal, &r, &Receiver::handler);
  s.emitSignal(&Sender::osignal);
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
  s.connectSignal(&Sender::osignal, &r,
                  &Receiver::typeHandler<TypeParam>);
  s.connectSignal(&Sender::osignal, &r,
                  &Receiver::otherTypeHandler<TypeParam>);
  s.disconnectSignal(&Sender::osignal, &r,
                     &Receiver::typeHandler<TypeParam>);
  s.emitSignal(&Sender::osignal, TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, disconnectAll)
{
  class Sender : public SenderBase {
  public:
    core::signal<> signal1, signal2;
    core::signal<const char*> signal3;
  };

  Sender s;
  Receiver r;
  Receiver r2;

  callCount = 0;
  s.connectSignal(&Sender::signal1, &r, &Receiver::handler);
  s.connectSignal(&Sender::signal2, &r, &Receiver::handler);
  s.connectSignal(&Sender::signal3, &r,
                  &Receiver::typeHandler<const char*>);
  s.connectSignal(&Sender::signal1, &r2, &Receiver::handler);
  s.disconnectSignals(&r);
  s.emitSignal(&Sender::signal1);
  s.emitSignal(&Sender::signal2);
  s.emitSignal(&Sender::signal3, "data");
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
    s.connectSignal(&Sender::isignal, &scoped_r, &Receiver::handler);
  }
  s.emitSignal(&Sender::isignal);
  EXPECT_EQ(callCount, 0);
}

TEST(Signals, disconnectBadSignal)
{
  class SenderA : public SenderBase {
  public:
    core::signal<> goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal<> badsignal;
  };

  SenderA s;
  Receiver r;

  s.disconnectSignal(&SenderA::goodsignal, &r, &Receiver::handler);
  EXPECT_THROW({
    s.disconnectSignal(&SenderB::badsignal, &r, &Receiver::handler);
  }, std::logic_error);
}

TYPED_TEST(SignalsArgs, disconnectBadSignalArg)
{
  class SenderA : public SenderBase {
  public:
    core::signal<TypeParam> goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal<TypeParam> badsignal;
  };

  SenderA s;
  Receiver r;

  s.disconnectSignal(&SenderA::goodsignal, &r,
                     &Receiver::typeHandler<TypeParam>);
  EXPECT_THROW({
    s.disconnectSignal(&SenderB::badsignal, &r,
                       &Receiver::typeHandler<TypeParam>);
  }, std::logic_error);
}

TEST(Signals, addWhileEmitting)
{
  class Sender : public SenderBase {
  public:
    core::signal<> asignal;
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
    core::signal<> rsignal;
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
  s.connectSignal(&Sender::inthandler, &r, &Receiver::typeHandler<int>);
  s.emitSignal(&Sender::inthandler, 1.2);
  EXPECT_EQ(callCount, 1);

  /* Sender casts to int */
  callCount = 0;
  s.connectSignal(&Sender::intemitter, &r, &Receiver::typeHandler<int>);
  s.emitSignal(&Sender::intemitter, 1.2);
  s.emitSignal(&Sender::intemitter, (unsigned long long)123);
  EXPECT_EQ(callCount, 2);
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
  s.connectSignal(&Sender::refhandler, &r,
                  &Receiver::typeHandler<const TypeParam&>);
  s.emitSignal(&Sender::refhandler, TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Sender adds reference */
  callCount = 0;
  s.connectSignal(&Sender::refemitter, &r,
                  &Receiver::typeHandler<const TypeParam&>);
  s.emitSignal(&Sender::refemitter, TestFixture::value);
  EXPECT_EQ(callCount, 1);
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

  /* Receiver adds pointer const qualifier */
  callCount = 0;
  s.connectSignal(&Sender::consthandler, &r,
                  &Receiver::typeHandler<const TypeParam*>);
  s.emitSignal(&Sender::consthandler, &TestFixture::value);
  EXPECT_EQ(callCount, 1);

  /* Sender adds pointer const qualifier */
  callCount = 0;
  s.connectSignal(&Sender::constemitter, &r,
                  &Receiver::typeHandler<const TypeParam*>);
  s.emitSignal(&Sender::constemitter, &TestFixture::value);
  EXPECT_EQ(callCount, 1);
}

TEST(Signals, emitBadSignal)
{
  class SenderA : public SenderBase {
  public:
    core::signal<> goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal<> badsignal;
  };

  SenderA s;

  s.emitSignal(&SenderA::goodsignal);
  EXPECT_THROW({
    s.emitSignal(&SenderB::badsignal);
  }, std::logic_error);
}

TYPED_TEST(SignalsArgs, emitBadSignalArg)
{
  class SenderA : public SenderBase {
  public:
    core::signal<TypeParam> goodsignal;
  };
  class SenderB : public SenderBase {
  public:
    core::signal<TypeParam> badsignal;
  };

  SenderA s;

  s.emitSignal(&SenderA::goodsignal, TestFixture::value);
  EXPECT_THROW({
    s.emitSignal(&SenderB::badsignal, TestFixture::value);
  }, std::logic_error);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
