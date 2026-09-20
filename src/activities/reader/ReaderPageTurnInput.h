#pragma once

#include <cstdint>

namespace ReaderUtils {

struct TouchPageTurn {
  bool prev = false;
  bool next = false;
  unsigned long heldMs = 0;
};

class TouchPageTurnFilter {
  TouchPageTurn pending;
  uint32_t pendingAt = 0;
  uint32_t swipeAt = 0;
  bool swipeSeen = false;

 public:
  static constexpr uint32_t MERGE_MS = 150;

  bool hasPending() const { return pending.prev || pending.next; }
  void clear() { *this = {}; }

  TouchPageTurn update(TouchPageTurn input, bool swipe, bool combined, uint32_t now, bool contactActive = false) {
    if (!combined) {
      clear();
      return input;
    }
    if (swipe) {
      pending = {};
      swipeAt = now;
      swipeSeen = true;
      return input;
    }
    if (input.prev || input.next) {
      if (swipeSeen && now - swipeAt <= MERGE_MS) return {};
      // Keep the first contact's deadline; repeated samples cannot postpone it.
      if (!hasPending()) pendingAt = now;
      pending = input;
    }
    if (!hasPending() || now - pendingAt < MERGE_MS) return {};
    // Let a following contact finish classification, with a bounded wait even
    // if the controller fails to report its release (SDK swipe limit: 700 ms).
    if (contactActive && now - pendingAt < MERGE_MS + 700) return {};
    const auto result = pending;
    pending = {};
    return result;
  }
};

}  // namespace ReaderUtils
