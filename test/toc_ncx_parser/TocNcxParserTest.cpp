#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "Epub/BookMetadataCache.h"
#include "TocNcxParser.h"

namespace {
std::string document(const std::string& prefix) {
  std::string xml = R"(<prefix:ncx xmlns="http://www.daisy.org/z3986/2005/ncx/"
      xmlns:prefix="http://www.daisy.org/z3986/2005/ncx/">
    <prefix:navMap>
      <prefix:navPoint>
        <prefix:navLabel><prefix:text>Chapter &amp; part</prefix:text></prefix:navLabel>
        <prefix:content src="Text/chapter1.xhtml#start"/>
        <prefix:navPoint>
          <prefix:navLabel><prefix:text>Section</prefix:text></prefix:navLabel>
          <prefix:content src="Text/chapter1.xhtml#section"/>
        </prefix:navPoint>
      </prefix:navPoint>
      <prefix:navPoint>
        <prefix:navLabel><prefix:text>Appendix</prefix:text></prefix:navLabel>
        <prefix:content src="Text/appendix.xhtml"/>
      </prefix:navPoint>
    </prefix:navMap>
  </prefix:ncx>)";
  const std::string marker = prefix.empty() ? "prefix:" : "prefix";
  for (size_t pos = 0; (pos = xml.find(marker, pos)) != std::string::npos; pos += prefix.size()) {
    xml.replace(pos, marker.size(), prefix);
  }
  return xml;
}

void parse(BookMetadataCache& cache, const std::string& xml, const size_t chunkSize = 1024) {
  const std::string basePath = "OPS/";
  TocNcxParser parser(basePath, xml.size(), &cache);
  ASSERT_TRUE(parser.setup());
  for (size_t offset = 0; offset < xml.size(); offset += chunkSize) {
    const auto count = std::min(chunkSize, xml.size() - offset);
    ASSERT_EQ(parser.write(reinterpret_cast<const uint8_t*>(xml.data() + offset), count), count);
  }
}

void expectEntries(const BookMetadataCache& cache) {
  ASSERT_EQ(cache.entries.size(), 3u);
  EXPECT_EQ(cache.entries[0].title, "Chapter & part");
  EXPECT_EQ(cache.entries[0].href, "OPS/Text/chapter1.xhtml");
  EXPECT_EQ(cache.entries[0].anchor, "start");
  EXPECT_EQ(cache.entries[0].level, 1);
  EXPECT_EQ(cache.entries[1].title, "Section");
  EXPECT_EQ(cache.entries[1].href, "OPS/Text/chapter1.xhtml");
  EXPECT_EQ(cache.entries[1].anchor, "section");
  EXPECT_EQ(cache.entries[1].level, 2);
  EXPECT_EQ(cache.entries[2].title, "Appendix");
  EXPECT_EQ(cache.entries[2].href, "OPS/Text/appendix.xhtml");
  EXPECT_TRUE(cache.entries[2].anchor.empty());
  EXPECT_EQ(cache.entries[2].level, 1);
}
}  // namespace

TEST(TocNcxParser, ReadsDefaultNamespace) {
  BookMetadataCache cache;
  parse(cache, document(""));
  expectEntries(cache);
}

TEST(TocNcxParser, ReadsPrefixedNamespace) {
  BookMetadataCache cache;
  parse(cache, document("ns0"), 1);
  expectEntries(cache);
}
