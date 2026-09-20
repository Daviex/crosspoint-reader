#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "activities/reader/ReaderUtils.h"

namespace {
using SwipeDir = MappedInputManager::SwipeDir;

class ReaderTouchTest : public testing::Test {
 protected:
  GfxRenderer renderer;
  MappedInputManager input;

  void SetUp() override {
    SETTINGS.touchReaderControls = CrossPointSettings::TOUCH_READER_TAP_SWIPE;
    gpio.heldMs = 0;
  }

  void tapAt(int x) {
    input.tap = true;
    input.tapX = x;
    input.tapY = 400;
  }

  void expectTurn(bool prev, bool next, unsigned long heldMs = 0) {
    const auto result = ReaderUtils::detectTouchPageTurn(renderer, input);
    EXPECT_EQ(result.prev, prev);
    EXPECT_EQ(result.next, next);
    EXPECT_EQ(result.heldMs, heldMs);
  }
};

TEST_F(ReaderTouchTest, TapThenSwipeOnSeparateTicksUsesSwipeDirectionOnly) {
  ReaderUtils::TouchPageTurnFilter filter;
  tapAt(20);
  EXPECT_FALSE(filter.update(ReaderUtils::detectTouchPageTurn(renderer, input), false, true, 1000).prev);
  EXPECT_FALSE(filter.update({}, false, true, 1200, true).prev);  // Following swipe still down.
  input.tap = false;
  input.swipe = SwipeDir::Left;
  const auto swipe = filter.update(ReaderUtils::detectTouchPageTurn(renderer, input), true, true, 1250);
  EXPECT_TRUE(swipe.next);
  EXPECT_FALSE(swipe.prev);
  EXPECT_FALSE(filter.hasPending());
  const auto idle = filter.update({}, false, true, 1300);
  EXPECT_FALSE(idle.prev || idle.next);
}

TEST_F(ReaderTouchTest, SwipeSuppressesNearbyTapButNotALaterTap) {
  ReaderUtils::TouchPageTurnFilter filter;
  input.swipe = SwipeDir::Left;
  EXPECT_TRUE(filter.update(ReaderUtils::detectTouchPageTurn(renderer, input), true, true, 1000).next);
  input.swipe = SwipeDir::None;
  tapAt(20);
  const auto adjacent = filter.update(ReaderUtils::detectTouchPageTurn(renderer, input), false, true, 1050);
  EXPECT_FALSE(adjacent.prev || adjacent.next);
  EXPECT_FALSE(filter.hasPending());
  filter.update(ReaderUtils::detectTouchPageTurn(renderer, input), false, true, 1300);
  EXPECT_TRUE(filter.update({}, false, true, 1450).prev);
  EXPECT_FALSE(filter.hasPending());
}

TEST_F(ReaderTouchTest, SingleTapDeadlineHoldDurationAndResetArePreserved) {
  ReaderUtils::TouchPageTurnFilter filter;
  const ReaderUtils::TouchPageTurn tap{false, true, ReaderUtils::SKIP_HOLD_MS};
  const uint32_t start = std::numeric_limits<uint32_t>::max() - 50;
  EXPECT_FALSE(filter.update(tap, false, true, start).next);
  EXPECT_FALSE(filter.update({}, false, true, start + 149).next);
  const auto ready = filter.update({}, false, true, start + 150);
  EXPECT_TRUE(ready.next);
  EXPECT_EQ(ready.heldMs, ReaderUtils::SKIP_HOLD_MS);
  EXPECT_FALSE(filter.update({}, false, true, start + 151).next);
  filter.update(tap, false, true, 2000);
  filter.clear();
  EXPECT_FALSE(filter.update({}, false, true, 2300).next);
  EXPECT_TRUE(filter.update(tap, false, false, 2400).next);  // Tap-only mode stays immediate.
  EXPECT_FALSE(filter.hasPending());
  filter.update(tap, false, true, 3000);
  EXPECT_TRUE(filter.update({}, false, true, 3850, true).next);  // A stuck contact cannot strand the tap.
}

TEST_F(ReaderTouchTest, CombinedModeAlternatesTapsAndSwipes) {
  tapAt(20);
  expectTurn(true, false);
  input.tap = false;
  input.swipe = SwipeDir::Left;
  expectTurn(false, true);
  input.swipe = SwipeDir::None;
  gpio.heldMs = ReaderUtils::SKIP_HOLD_MS;
  tapAt(460);
  expectTurn(false, true, gpio.heldMs);
  input.tap = false;
  input.swipe = SwipeDir::Right;
  expectTurn(true, false);
  input.swipe = SwipeDir::None;
  expectTurn(false, false);
  gpio.heldMs = 0;
  tapAt(240);
  expectTurn(false, false);  // The center remains free for the reader menu.
}

TEST_F(ReaderTouchTest, SwipesUseDirectionWithoutPropagatingHoldTime) {
  gpio.heldMs = ReaderUtils::SKIP_HOLD_MS;
  for (const auto direction : {SwipeDir::Left, SwipeDir::Right, SwipeDir::Up, SwipeDir::Down}) {
    SCOPED_TRACE(static_cast<int>(direction));
    input.swipe = direction;
    expectTurn(direction == SwipeDir::Right, direction == SwipeDir::Left);
  }
}

TEST_F(ReaderTouchTest, SwipeOnlyStillIgnoresTaps) {
  SETTINGS.touchReaderControls = CrossPointSettings::TOUCH_READER_SWIPE;
  tapAt(460);
  expectTurn(false, false);
  input.tap = false;
  input.swipe = SwipeDir::Left;
  expectTurn(false, true);
  input.swipe = SwipeDir::Right;
  expectTurn(true, false);
}

TEST_F(ReaderTouchTest, TapModesKeepTheirDirectionsAndIgnoreSwipes) {
  for (const auto mode : {CrossPointSettings::TOUCH_READER_ON, CrossPointSettings::TOUCH_READER_INVERTED_TAP}) {
    SCOPED_TRACE(static_cast<int>(mode));
    SETTINGS.touchReaderControls = mode;
    const bool inverted = mode == CrossPointSettings::TOUCH_READER_INVERTED_TAP;
    input.swipe = SwipeDir::None;
    tapAt(20);
    expectTurn(!inverted, inverted);
    tapAt(460);
    expectTurn(inverted, !inverted);
    input.tap = false;
    input.swipe = SwipeDir::Left;
    expectTurn(false, false);
    input.swipe = SwipeDir::Right;
    expectTurn(false, false);
  }
}
}  // namespace
