#pragma once

#include "util/HomeButtonInput.h"

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, PageBack, PageForward, Power };
  enum class SwipeDir { None, Left, Right, Up, Down };

  bool touchAvailable = true;
  bool tap = false;
  int tapX = 0;
  int tapY = 0;
  SwipeDir swipe = SwipeDir::None;
  bool menuGesture = false;
  bool readerMenuSwipeUp = false;
  bool backGesture = false;

  bool hasTouch() const { return touchAvailable; }
  bool wasScreenTapped(int& x, int& y) const {
    if (!tap) return false;
    x = tapX;
    y = tapY;
    return true;
  }
  SwipeDir wasSwipe() const { return swipe; }
  bool wasMenuGesture() const { return menuGesture; }
  bool wasReaderMenuSwipeUp() const { return readerMenuSwipeUp; }
  bool wasBackGesture() const { return backGesture; }
  bool isNavDirectionSwapped() const { return false; }
  bool wasPressed(Button) const { return false; }
  bool wasReleased(Button button) const { return button == Button::Back && backGesture; }
  bool wasLongPressed(Button, unsigned long) const { return false; }
  unsigned long getHeldTime() const { return 0; }
  HomeButtonAction homeButtonAction() const { return HomeButtonAction::Ignore; }
};
