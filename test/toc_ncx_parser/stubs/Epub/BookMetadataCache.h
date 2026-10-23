#pragma once

#include <cstdint>
#include <string>
#include <vector>

class BookMetadataCache {
 public:
  struct Entry {
    std::string title;
    std::string href;
    std::string anchor;
    uint8_t level;
  };

  std::vector<Entry> entries;

  void createTocEntry(const std::string& title, const std::string& href, const std::string& anchor,
                      const uint8_t level) {
    entries.push_back({title, href, anchor, level});
  }
};
