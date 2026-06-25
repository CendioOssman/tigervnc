/* Copyright 2020-2025 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <unistd.h>

#include <vector>

#include <gtest/gtest.h>

#include "../../vncviewer/GestureHandler.h"

class TestClass : public GestureCallback
{
  protected:
    void handleGestureEvent(const GestureEvent& event) override;

  public:
    std::vector<GestureEvent> events;
};

void TestClass::handleGestureEvent(const GestureEvent& event)
{
  events.push_back(event);
}

TEST(GestureHandler, oneTapNormal)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureOneTap);
  EXPECT_EQ(test.events[0].eventX, 20.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);

  EXPECT_EQ(test.events[1].type, GestureEnd);
  EXPECT_EQ(test.events[1].gesture, GestureOneTap);
  EXPECT_EQ(test.events[1].eventX, 20.0);
  EXPECT_EQ(test.events[1].eventY, 30.0);
}

TEST(GestureHandler, twoTapNormal)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 50.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureTwoTap);
  EXPECT_EQ(test.events[0].eventX, 25.0);
  EXPECT_EQ(test.events[0].eventY, 40.0);

  EXPECT_EQ(test.events[1].type, GestureEnd);
  EXPECT_EQ(test.events[1].gesture, GestureTwoTap);
  EXPECT_EQ(test.events[1].eventX, 25.0);
  EXPECT_EQ(test.events[1].eventY, 40.0);
}

TEST(GestureHandler, twoTapSlowBegin)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);

  usleep(500000);
  core::Timer::checkTimeouts();

  handler.handleTouchBegin(2, 30.0, 50.0);
  handler.handleTouchEnd(1);
  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, twoTapSlowEnd)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 50.0);
  handler.handleTouchEnd(1);

  usleep(500000);
  core::Timer::checkTimeouts();

  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, twoTapTimeout)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 50.0);

  usleep(1500000);
  core::Timer::checkTimeouts();

  handler.handleTouchEnd(1);
  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, threeTapNormal)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 50.0);
  handler.handleTouchBegin(3, 40.0, 40.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchEnd(3);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureThreeTap);
  EXPECT_EQ(test.events[0].eventX, 30.0);
  EXPECT_EQ(test.events[0].eventY, 40.0);

  EXPECT_EQ(test.events[1].type, GestureEnd);
  EXPECT_EQ(test.events[1].gesture, GestureThreeTap);
  EXPECT_EQ(test.events[1].eventX, 30.0);
  EXPECT_EQ(test.events[1].eventY, 40.0);
}

TEST(GestureHandler, threeTapSlowBegin)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 50.0);

  usleep(500000);
  core::Timer::checkTimeouts();

  handler.handleTouchBegin(3, 40.0, 40.0);
  handler.handleTouchEnd(1);
  handler.handleTouchEnd(2);
  handler.handleTouchEnd(3);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, threeTapSlowEnd)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 50.0);
  handler.handleTouchBegin(3, 40.0, 40.0);
  handler.handleTouchEnd(1);
  handler.handleTouchEnd(2);

  usleep(500000);
  core::Timer::checkTimeouts();

  handler.handleTouchEnd(3);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, threeTapDrag)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 50.0);
  handler.handleTouchBegin(3, 40.0, 40.0);

  handler.handleTouchUpdate(1, 120.0, 130.0);
  handler.handleTouchUpdate(2, 130.0, 150.0);
  handler.handleTouchUpdate(3, 140.0, 140.0);

  handler.handleTouchEnd(1);
  handler.handleTouchEnd(2);
  handler.handleTouchEnd(3);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, threeTapTimeout)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 50.0);
  handler.handleTouchBegin(3, 40.0, 40.0);

  usleep(1500000);
  core::Timer::checkTimeouts();

  handler.handleTouchEnd(1);
  handler.handleTouchEnd(2);
  handler.handleTouchEnd(3);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, dragHoriz)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 40.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 80.0, 30.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 20.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureDrag);
  EXPECT_EQ(test.events[1].eventX, 80.0);
  EXPECT_EQ(test.events[1].eventY, 30.0);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 80.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);
}

TEST(GestureHandler, dragVert)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 20.0, 50.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 20.0, 90.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 20.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureDrag);
  EXPECT_EQ(test.events[1].eventX, 20.0);
  EXPECT_EQ(test.events[1].eventY, 90.0);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 20.0);
  EXPECT_EQ(test.events[0].eventY, 90.0);
}

TEST(GestureHandler, dragDiag)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 120.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 90.0, 100.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 60.0, 70.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 120.0);
  EXPECT_EQ(test.events[0].eventY, 130.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureDrag);
  EXPECT_EQ(test.events[1].eventX, 60.0);
  EXPECT_EQ(test.events[1].eventY, 70.0);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 60.0);
  EXPECT_EQ(test.events[0].eventY, 70.0);
}

TEST(GestureHandler, longPressNormal)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  usleep(1500000);
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureLongPress);
  EXPECT_EQ(test.events[0].eventX, 20.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureLongPress);
  EXPECT_EQ(test.events[0].eventX, 20.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);
}

TEST(GestureHandler, longPressDrag)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  usleep(1500000);
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureLongPress);
  EXPECT_EQ(test.events[0].eventX, 20.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);

  test.events.clear();

  handler.handleTouchUpdate(1, 120.0, 50.0);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureUpdate);
  EXPECT_EQ(test.events[0].gesture, GestureLongPress);
  EXPECT_EQ(test.events[0].eventX, 120.0);
  EXPECT_EQ(test.events[0].eventY, 50.0);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureLongPress);
  EXPECT_EQ(test.events[0].eventX, 120.0);
  EXPECT_EQ(test.events[0].eventY, 50.0);
}

TEST(GestureHandler, twoDragFastDistinctHoriz)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 40.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(2, 50.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(2, 90.0, 30.0);
  handler.handleTouchUpdate(1, 80.0, 30.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 25.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);
  EXPECT_EQ(test.events[0].magnitudeX, 0.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[1].eventX, 25.0);
  EXPECT_EQ(test.events[1].eventY, 30.0);
  EXPECT_EQ(test.events[1].magnitudeX, 60.0);
  EXPECT_EQ(test.events[1].magnitudeY, 0.0);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 25.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);
  EXPECT_EQ(test.events[0].magnitudeX, 60.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);
}

TEST(GestureHandler, twoDragFastDistinctVert)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 20.0, 100.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(2, 30.0, 40.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(2, 30.0, 90.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 25.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);
  EXPECT_EQ(test.events[0].magnitudeX, 0.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[1].eventX, 25.0);
  EXPECT_EQ(test.events[1].eventY, 30.0);
  EXPECT_EQ(test.events[1].magnitudeX, 0.0);
  EXPECT_EQ(test.events[1].magnitudeY, 65.0);

  test.events.clear();

  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 25.0);
  EXPECT_EQ(test.events[0].eventY, 30.0);
  EXPECT_EQ(test.events[0].magnitudeX, 0.0);
  EXPECT_EQ(test.events[0].magnitudeY, 65.0);
}

TEST(GestureHandler, twoDragFastDistinctDiag)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 120.0, 130.0);
  handler.handleTouchBegin(2, 130.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 80.0, 90.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(2, 100.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(2, 60.0, 70.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 125.0);
  EXPECT_EQ(test.events[0].eventY, 130.0);
  EXPECT_EQ(test.events[0].magnitudeX, 0.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[1].eventX, 125.0);
  EXPECT_EQ(test.events[1].eventY, 130.0);
  EXPECT_EQ(test.events[1].magnitudeX, -55.0);
  EXPECT_EQ(test.events[1].magnitudeY, -50.0);

  test.events.clear();

  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 125.0);
  EXPECT_EQ(test.events[0].eventY, 130.0);
  EXPECT_EQ(test.events[0].magnitudeX, -55.0);
  EXPECT_EQ(test.events[0].magnitudeY, -50.0);
}

TEST(GestureHandler, twoDragFastAlmost)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 30.0);
  handler.handleTouchUpdate(1, 80.0, 30.0);
  handler.handleTouchUpdate(2, 70.0, 30.0);
  handler.handleTouchEnd(1);
  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 0);

  usleep(500000);
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, twoDragSlowHoriz)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 50.0, 40.0);
  handler.handleTouchBegin(2, 60.0, 40.0);
  handler.handleTouchUpdate(2, 80.0, 40.0);
  handler.handleTouchUpdate(1, 110.0, 40.0);

  ASSERT_EQ(test.events.size(), 0);

  usleep(60000); // 60ms
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 55.0);
  EXPECT_EQ(test.events[0].eventY, 40.0);
  EXPECT_EQ(test.events[0].magnitudeX, 0.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[1].eventX, 55.0);
  EXPECT_EQ(test.events[1].eventY, 40.0);
  EXPECT_EQ(test.events[1].magnitudeX, 40.0);
  EXPECT_EQ(test.events[1].magnitudeY, 0.0);
}

TEST(GestureHandler, twoDragSlowVert)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 40.0, 40.0);
  handler.handleTouchBegin(2, 40.0, 60.0);
  handler.handleTouchUpdate(2, 40.0, 80.0);
  handler.handleTouchUpdate(1, 40.0, 100.0);

  ASSERT_EQ(test.events.size(), 0);

  usleep(60000); // 60ms
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 40.0);
  EXPECT_EQ(test.events[0].eventY, 50.0);
  EXPECT_EQ(test.events[0].magnitudeX, 0.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[1].eventX, 40.0);
  EXPECT_EQ(test.events[1].eventY, 50.0);
  EXPECT_EQ(test.events[1].magnitudeX, 0.0);
  EXPECT_EQ(test.events[1].magnitudeY, 40.0);
}

TEST(GestureHandler, twoDragSlowDiag)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 50.0, 40.0);
  handler.handleTouchBegin(2, 40.0, 60.0);
  handler.handleTouchUpdate(1, 70.0, 60.0);
  handler.handleTouchUpdate(2, 90.0, 110.0);

  ASSERT_EQ(test.events.size(), 0);

  usleep(60000); // 60ms
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[0].eventX, 45.0);
  EXPECT_EQ(test.events[0].eventY, 50.0);
  EXPECT_EQ(test.events[0].magnitudeX, 0.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureTwoDrag);
  EXPECT_EQ(test.events[1].eventX, 45.0);
  EXPECT_EQ(test.events[1].eventY, 50.0);
  EXPECT_EQ(test.events[1].magnitudeX, 35.0);
  EXPECT_EQ(test.events[1].magnitudeY, 35.0);
}

TEST(GestureHandler, twoDragTooSlow)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);

  usleep(500000);
  core::Timer::checkTimeouts();

  handler.handleTouchBegin(2, 30.0, 30.0);
  handler.handleTouchUpdate(2, 50.0, 30.0);
  handler.handleTouchUpdate(1, 80.0, 30.0);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, pinchFastDistinctIn)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 0.0, 0.0);
  handler.handleTouchBegin(2, 130.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 50.0, 40.0);
  handler.handleTouchUpdate(2, 100.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(2, 60.0, 70.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GesturePinch);
  EXPECT_EQ(test.events[0].eventX, 65.0);
  EXPECT_EQ(test.events[0].eventY, 65.0);
  EXPECT_EQ(test.events[0].magnitudeX, 130.0);
  EXPECT_EQ(test.events[0].magnitudeY, 130.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GesturePinch);
  EXPECT_EQ(test.events[1].eventX, 65.0);
  EXPECT_EQ(test.events[1].eventY, 65.0);
  EXPECT_EQ(test.events[1].magnitudeX, 10.0);
  EXPECT_EQ(test.events[1].magnitudeY, 30.0);

  test.events.clear();

  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GesturePinch);
  EXPECT_EQ(test.events[0].eventX, 65.0);
  EXPECT_EQ(test.events[0].eventY, 65.0);
  EXPECT_EQ(test.events[0].magnitudeX, 10.0);
  EXPECT_EQ(test.events[0].magnitudeY, 30.0);
}

TEST(GestureHandler, pinchFastDistinctOut)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 100.0, 100.0);
  handler.handleTouchBegin(2, 110.0, 100.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 130.0, 70.0);
  handler.handleTouchUpdate(2, 0.0, 200.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 180.0, 20.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GesturePinch);
  EXPECT_EQ(test.events[0].eventX, 105.0);
  EXPECT_EQ(test.events[0].eventY, 100.0);
  EXPECT_EQ(test.events[0].magnitudeX, 10.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GesturePinch);
  EXPECT_EQ(test.events[1].eventX, 105.0);
  EXPECT_EQ(test.events[1].eventY, 100.0);
  EXPECT_EQ(test.events[1].magnitudeX, 180.0);
  EXPECT_EQ(test.events[1].magnitudeY, 180.0);

  test.events.clear();

  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GesturePinch);
  EXPECT_EQ(test.events[0].eventX, 105.0);
  EXPECT_EQ(test.events[0].eventY, 100.0);
  EXPECT_EQ(test.events[0].magnitudeX, 180.0);
  EXPECT_EQ(test.events[0].magnitudeY, 180.0);
}

TEST(GestureHandler, pinchFastAlmost)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 130.0, 130.0);
  handler.handleTouchUpdate(1, 80.0, 70.0);
  handler.handleTouchEnd(1);
  handler.handleTouchEnd(2);

  ASSERT_EQ(test.events.size(), 0);

  usleep(500000);
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, pinchSlowIn)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 0.0, 0.0);
  handler.handleTouchBegin(2, 130.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 50.0, 40.0);
  handler.handleTouchUpdate(2, 100.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  usleep(60000); // 60ms
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GesturePinch);
  EXPECT_EQ(test.events[0].eventX, 65.0);
  EXPECT_EQ(test.events[0].eventY, 65.0);
  EXPECT_EQ(test.events[0].magnitudeX, 130.0);
  EXPECT_EQ(test.events[0].magnitudeY, 130.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GesturePinch);
  EXPECT_EQ(test.events[1].eventX, 65.0);
  EXPECT_EQ(test.events[1].eventY, 65.0);
  EXPECT_EQ(test.events[1].magnitudeX, 50.0);
  EXPECT_EQ(test.events[1].magnitudeY, 90.0);
}

TEST(GestureHandler, pinchSlowOut)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 100.0, 130.0);
  handler.handleTouchBegin(2, 110.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(2, 200.0, 130.0);

  ASSERT_EQ(test.events.size(), 0);

  usleep(60000); // 60ms
  core::Timer::checkTimeouts();

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GesturePinch);
  EXPECT_EQ(test.events[0].eventX, 105.0);
  EXPECT_EQ(test.events[0].eventY, 130.0);
  EXPECT_EQ(test.events[0].magnitudeX, 10.0);
  EXPECT_EQ(test.events[0].magnitudeY, 0.0);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GesturePinch);
  EXPECT_EQ(test.events[1].eventX, 105.0);
  EXPECT_EQ(test.events[1].eventY, 130.0);
  EXPECT_EQ(test.events[1].magnitudeX, 100.0);
  EXPECT_EQ(test.events[1].magnitudeY, 0.0);
}

TEST(GestureHandler, pinchTooSlow)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 0.0, 0.0);

  usleep(60000); // 60ms
  core::Timer::checkTimeouts();

  handler.handleTouchBegin(2, 130.0, 130.0);
  handler.handleTouchUpdate(2, 100.0, 130.0);
  handler.handleTouchUpdate(1, 50.0, 40.0);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, extraIgnore)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchUpdate(1, 40.0, 30.0);
  handler.handleTouchUpdate(1, 80.0, 30.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureDrag);

  test.events.clear();

  handler.handleTouchBegin(2, 10.0, 10.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 100.0, 50.0);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureUpdate);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 100.0);
  EXPECT_EQ(test.events[0].eventY, 50.0);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 100.0);
  EXPECT_EQ(test.events[0].eventY, 50.0);
}

TEST(GestureHandler, ignoreWhenAwaitingGestureEnd)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchBegin(2, 30.0, 30.0);
  handler.handleTouchUpdate(1, 40.0, 30.0);
  handler.handleTouchUpdate(2, 90.0, 30.0);
  handler.handleTouchUpdate(1, 80.0, 30.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureTwoDrag);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureTwoDrag);

  test.events.clear();

  handler.handleTouchBegin(3, 10.0, 10.0);
  handler.handleTouchEnd(3);

  ASSERT_EQ(test.events.size(), 0);
}

TEST(GestureHandler, ignoreAfterGesture)
{
  TestClass test;
  GestureHandler handler(&test);

  handler.handleTouchBegin(1, 20.0, 30.0);
  handler.handleTouchUpdate(1, 40.0, 30.0);
  handler.handleTouchUpdate(1, 80.0, 30.0);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);

  EXPECT_EQ(test.events[1].type, GestureUpdate);
  EXPECT_EQ(test.events[1].gesture, GestureDrag);

  test.events.clear();

  // Start ignored event
  handler.handleTouchBegin(2, 10.0, 10.0);

  ASSERT_EQ(test.events.size(), 0);

  handler.handleTouchUpdate(1, 100.0, 50.0);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureUpdate);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 100.0);
  EXPECT_EQ(test.events[0].eventY, 50.0);

  test.events.clear();

  handler.handleTouchEnd(1);

  ASSERT_EQ(test.events.size(), 1);

  EXPECT_EQ(test.events[0].type, GestureEnd);
  EXPECT_EQ(test.events[0].gesture, GestureDrag);
  EXPECT_EQ(test.events[0].eventX, 100.0);
  EXPECT_EQ(test.events[0].eventY, 50.0);

  // End ignored event
  handler.handleTouchEnd(2);

  // Check that everything is reseted after trailing ignores are released
  test.events.clear();

  handler.handleTouchBegin(3, 20.0, 30.0);
  handler.handleTouchEnd(3);

  ASSERT_EQ(test.events.size(), 2);

  EXPECT_EQ(test.events[0].type, GestureBegin);
  EXPECT_EQ(test.events[0].gesture, GestureOneTap);
  EXPECT_EQ(test.events[1].type, GestureEnd);
  EXPECT_EQ(test.events[1].gesture, GestureOneTap);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
