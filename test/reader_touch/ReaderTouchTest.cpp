#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "activities/reader/ReaderUtils.h"

namespace {
using SwipeDir = MappedInputManager::SwipeDir;

class ReaderTouchTest : public testing::Test {
 protected:
  GfxRenderer renderer;
  MappedInputManager input;

  void SetUp() override {
    SETTINGS.touchReaderControls = CrossPointSettings::TOUCH_READER_TAP_SWIPE;
    SETTINGS.showReaderMenu = CrossPointSettings::READER_MENU_TAP;
    gpio.heldMs = 0;
  }

  void tapAt(int x, int y) {
    input.tap = true;
    input.tapX = x;
    input.tapY = y;
  }

  void expectTurn(bool prev, bool next, unsigned long heldMs = 0) {
    const auto result = ReaderUtils::detectTouchPageTurn(renderer, input);
    EXPECT_EQ(result.prev, prev);
    EXPECT_EQ(result.next, next);
    EXPECT_EQ(result.heldMs, heldMs);
  }
};

TEST_F(ReaderTouchTest, ExistingSavedModeIdsRemainCompatible) {
  EXPECT_EQ(CrossPointSettings::TOUCH_READER_OFF, 0);
  EXPECT_EQ(CrossPointSettings::TOUCH_READER_ON, 1);
  EXPECT_EQ(CrossPointSettings::TOUCH_READER_SWIPE, 2);
  EXPECT_EQ(CrossPointSettings::TOUCH_READER_INVERTED_TAP, 3);
  EXPECT_EQ(CrossPointSettings::TOUCH_READER_TAP_SWIPE, 4);
}

TEST_F(ReaderTouchTest, CombinedModeAlternatesTapAndSwipeWithoutChangingSettings) {
  tapAt(20, 400);
  expectTurn(true, false);
  input.tap = false;
  input.swipe = SwipeDir::Left;
  expectTurn(false, true);
  input.swipe = SwipeDir::None;
  tapAt(460, 400);
  expectTurn(false, true);
  input.tap = false;
  input.swipe = SwipeDir::Right;
  expectTurn(true, false);
  input.swipe = SwipeDir::None;
  expectTurn(false, false);
}

TEST_F(ReaderTouchTest, SwipeTakesPriorityOverConflictingTapAndNeverBecomesChapterSkip) {
  // Even if an input source reports both, one contact yields only one direction.
  gpio.heldMs = ReaderUtils::SKIP_HOLD_MS;
  tapAt(20, 400);
  input.swipe = SwipeDir::Left;
  expectTurn(false, true);
  tapAt(460, 400);
  input.swipe = SwipeDir::Right;
  expectTurn(true, false);
}

TEST_F(ReaderTouchTest, VerticalSwipesNeverFallThroughToPageTapZones) {
  tapAt(460, 400);
  for (const auto direction : {SwipeDir::Up, SwipeDir::Down}) {
    input.swipe = direction;
    expectTurn(false, false);
  }
}

TEST_F(ReaderTouchTest, TapRetainsExistingLongPressDuration) {
  gpio.heldMs = ReaderUtils::SKIP_HOLD_MS + 100;
  tapAt(460, 400);
  expectTurn(false, true, gpio.heldMs);
}

TEST_F(ReaderTouchTest, SwipeOnlyIgnoresTapsButAcceptsBothHorizontalDirections) {
  SETTINGS.touchReaderControls = CrossPointSettings::TOUCH_READER_SWIPE;
  gpio.heldMs = ReaderUtils::SKIP_HOLD_MS;
  tapAt(460, 400);
  expectTurn(false, false);
  input.tap = false;
  input.swipe = SwipeDir::Left;
  expectTurn(false, true);
  input.swipe = SwipeDir::Right;
  expectTurn(true, false);
  input.swipe = SwipeDir::Up;
  expectTurn(false, false);
}

TEST_F(ReaderTouchTest, TapOnlyAndInvertedTapKeepTheirDirectionsAndIgnoreSwipes) {
  for (const uint8_t mode : {CrossPointSettings::TOUCH_READER_ON, CrossPointSettings::TOUCH_READER_INVERTED_TAP}) {
    SETTINGS.touchReaderControls = mode;
    const bool inverted = mode == CrossPointSettings::TOUCH_READER_INVERTED_TAP;
    input.swipe = SwipeDir::None;
    tapAt(20, 400);
    expectTurn(!inverted, inverted);
    tapAt(460, 400);
    expectTurn(inverted, !inverted);
    input.tap = false;
    input.swipe = SwipeDir::Left;
    expectTurn(false, false);
    input.swipe = SwipeDir::Right;
    expectTurn(false, false);
  }
}

TEST_F(ReaderTouchTest, TouchOffDisablesBothPageGestures) {
  SETTINGS.touchReaderControls = CrossPointSettings::TOUCH_READER_OFF;
  tapAt(460, 400);
  expectTurn(false, false);
  input.swipe = SwipeDir::Left;
  expectTurn(false, false);
}

TEST_F(ReaderTouchTest, BoardsWithoutTouchIgnoreAllPageAndMenuGestures) {
  input.touchAvailable = false;
  input.menuGesture = true;
  input.readerMenuSwipeUp = true;
  tapAt(460, 400);
  input.swipe = SwipeDir::Left;
  for (uint8_t mode = 0; mode < CrossPointSettings::TOUCH_READER_CONTROLS_COUNT; ++mode) {
    SETTINGS.touchReaderControls = mode;
    expectTurn(false, false);
    EXPECT_FALSE(ReaderUtils::isTouchMenuGesture(renderer, input));
  }
}

TEST_F(ReaderTouchTest, PageAndMenuZonesUseCurrentLogicalDimensionsAndStayDisjoint) {
  // Logical coordinates arrive already rotated by MappedInputManager.
  for (const auto size : {std::array{480, 800}, std::array{800, 480}, std::array{481, 801}}) {
    renderer.width = size[0];
    renderer.height = size[1];
    const int leftEnd = renderer.width / 3;
    const int rightStart = renderer.width - leftEnd;
    tapAt(leftEnd - 1, renderer.height / 2);
    expectTurn(true, false);
    EXPECT_FALSE(ReaderUtils::isTouchMenuGesture(renderer, input));
    tapAt(leftEnd, renderer.height / 2);
    expectTurn(false, false);
    EXPECT_TRUE(ReaderUtils::isTouchMenuGesture(renderer, input));
    tapAt(rightStart - 1, renderer.height / 2);
    expectTurn(false, false);
    EXPECT_TRUE(ReaderUtils::isTouchMenuGesture(renderer, input));
    tapAt(rightStart, renderer.height / 2);
    expectTurn(false, true);
    EXPECT_FALSE(ReaderUtils::isTouchMenuGesture(renderer, input));
    tapAt(renderer.width / 2, 0);
    expectTurn(false, false);
    EXPECT_FALSE(ReaderUtils::isTouchMenuGesture(renderer, input));
    tapAt(renderer.width / 2, renderer.height - 1);
    expectTurn(false, false);
    EXPECT_FALSE(ReaderUtils::isTouchMenuGesture(renderer, input));
  }
}

TEST_F(ReaderTouchTest, MenuPreferenceRemainsIndependentOfPageTurnMode) {
  for (const uint8_t mode : {CrossPointSettings::TOUCH_READER_OFF, CrossPointSettings::TOUCH_READER_TAP_SWIPE}) {
    SETTINGS.touchReaderControls = mode;
    tapAt(240, 400);
    SETTINGS.showReaderMenu = CrossPointSettings::READER_MENU_TAP;
    EXPECT_TRUE(ReaderUtils::isTouchMenuGesture(renderer, input));
    SETTINGS.showReaderMenu = CrossPointSettings::READER_MENU_OFF;
    EXPECT_FALSE(ReaderUtils::isTouchMenuGesture(renderer, input));
    SETTINGS.showReaderMenu = CrossPointSettings::READER_MENU_SWIPE_UP;
    EXPECT_FALSE(ReaderUtils::isTouchMenuGesture(renderer, input));
    input.tap = false;
    input.swipe = SwipeDir::Up;
    input.readerMenuSwipeUp = true;
    expectTurn(false, false);
    EXPECT_TRUE(ReaderUtils::isTouchMenuGesture(renderer, input));
    input.readerMenuSwipeUp = false;
    input.swipe = SwipeDir::None;
  }
}

TEST_F(ReaderTouchTest, EdgeMenuSwipeRemainsAvailable) {
  input.menuGesture = true;
  input.swipe = SwipeDir::Down;
  expectTurn(false, false);
  EXPECT_TRUE(ReaderUtils::isTouchMenuGesture(renderer, input));
}

TEST_F(ReaderTouchTest, LeftEdgeRightSwipePagesBackWithoutLeavingReader) {
  input.backGesture = true;
  input.swipe = SwipeDir::Right;
  expectTurn(true, false);
  ActivityManager manager;
  bool homeCalled = false;
  const ReaderUtils::BackNavCallback home{&homeCalled, [](void* context) { *static_cast<bool*>(context) = true; }};
  EXPECT_FALSE(ReaderUtils::handleBackNavigation(input, manager, "/book.epub", home));
  EXPECT_FALSE(homeCalled);
}
}  // namespace
