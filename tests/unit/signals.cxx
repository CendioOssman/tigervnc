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

    void registerSignal(const char *name)
    {
        core::Object::registerSignal(name);
    }
    void emitSignal(const char *name)
    {
        core::Object::emitSignal(name);
    }
};

class Receiver : public core::Object {
public:
    Receiver() {}

    void genericHandler(Object*, const char*) { count++; }
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

    return 0;
}
