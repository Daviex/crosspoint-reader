#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "ChapterXPathResolver.h"
#include "ProgressMapper.h"

namespace {

constexpr char NESTED_CHAPTER[] =
    "<html><head><title>ignored</title></head><body><div><p>alpha <em>beta</em> omega</p></div></body></html>";

std::shared_ptr<Epub> makeEpub(const std::string& chapter, const size_t chunkSize = 0) {
  return std::make_shared<Epub>(std::vector<std::string>{chapter}, chunkSize);
}

CrossPointPosition positionAt(const uint32_t offset) {
  CrossPointPosition position{};
  position.spineIndex = 0;
  position.pageNumber = 1;
  position.totalPages = 10;
  position.paragraphIndex = 1;
  position.hasParagraphIndex = true;
  position.visibleTextOffset = offset;
  position.hasVisibleTextOffset = true;
  return position;
}

TEST(KOReaderPosition, ExportsExactInlineOffsetBeforeParagraphFallback) {
  const auto saved = ProgressMapper::toSavedProgress(makeEpub(NESTED_CHAPTER), positionAt(8));
  EXPECT_EQ(saved.xpath, "/body/DocFragment[1]/body/div[1]/p[1]/em[1]/text()[1].2");
}

TEST(KOReaderPosition, PreservesZeroOffsetOfTheSecondDirectTextNode) {
  const auto saved = ProgressMapper::toSavedProgress(makeEpub(NESTED_CHAPTER), positionAt(10));
  EXPECT_EQ(saved.xpath, "/body/DocFragment[1]/body/div[1]/p[1]/text()[2].0");
}

TEST(KOReaderPosition, ProducesTheSameAnchorAcrossByteChunks) {
  for (size_t chunkSize = 1; chunkSize <= 17; ++chunkSize) {
    SCOPED_TRACE(chunkSize);
    const auto epub = makeEpub(NESTED_CHAPTER, chunkSize);
    EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(8)).xpath,
              "/body/DocFragment[1]/body/div[1]/p[1]/em[1]/text()[1].2");
    EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(10)).xpath,
              "/body/DocFragment[1]/body/div[1]/p[1]/text()[2].0");
  }
}

TEST(KOReaderPosition, CountsUnicodeCodepointsAndSkipsNonVisibleElements) {
  const auto epub = makeEpub("<html><body>é<script>hidden</script>猫<p>e&#x301;😀x</p></body></html>", 1);
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(1)).xpath, "/body/DocFragment[1]/body/text()[2].0");
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(5)).xpath, "/body/DocFragment[1]/body/p[1]/text()[1].3");
}

TEST(KOReaderPosition, NormalizesLineEndingsAndCountsBodyWhitespace) {
  const std::string chapter = "<html><body>\r\n<p>a\r\nb\t c</p>\r</body></html>";
  for (size_t chunkSize = 1; chunkSize <= 9; ++chunkSize) {
    SCOPED_TRACE(chunkSize);
    const auto epub = makeEpub(chapter, chunkSize);
    EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(0)).xpath, "/body/DocFragment[1]/body/text()[1].0");
    EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(3)).xpath, "/body/DocFragment[1]/body/p[1]/text()[1].2");
    EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(7)).xpath, "/body/DocFragment[1]/body/text()[2].0");
  }
}

TEST(KOReaderPosition, CountsBuiltinAndNumericEntitiesAsDecodedCodepoints) {
  const std::string chapter = "<html><body><p>a&amp;&#x732B;&#160;&#128512;<em>x</em>z</p></body></html>";
  for (size_t chunkSize = 1; chunkSize <= 13; ++chunkSize) {
    SCOPED_TRACE(chunkSize);
    const auto epub = makeEpub(chapter, chunkSize);
    EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(4)).xpath, "/body/DocFragment[1]/body/p[1]/text()[1].4");
    EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(5)).xpath,
              "/body/DocFragment[1]/body/p[1]/em[1]/text()[1].0");
    EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(6)).xpath, "/body/DocFragment[1]/body/p[1]/text()[2].0");
  }
}

TEST(KOReaderPosition, ResolvesHtmlEntitiesWithFirmwareExpatConfiguration) {
  const auto epub = makeEpub("<!DOCTYPE html [<!ENTITY nbsp '&#160;'>]><html><body><p>a&nbsp;b</p></body></html>", 1);
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(2)).xpath, "/body/DocFragment[1]/body/p[1]/text()[1].2");
}

TEST(KOReaderPosition, ResolvesBodyDivAndListTextWithoutParagraphs) {
  const auto epub = makeEpub("<html><body>a<div>b</div><ul><li>c</li><li>d</li></ul>e</body></html>", 1);
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(1)).xpath, "/body/DocFragment[1]/body/div[1]/text()[1].0");
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(3)).xpath,
            "/body/DocFragment[1]/body/ul[1]/li[2]/text()[1].0");
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(4)).xpath, "/body/DocFragment[1]/body/text()[2].0");
}

TEST(KOReaderPosition, ResolvesDocumentEndToTheLastTextNode) {
  const auto epub = makeEpub("<html><body><p>abc</p></body></html>", 1);
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, positionAt(3)).xpath, "/body/DocFragment[1]/body/p[1]/text()[1].3");
}

TEST(KOReaderPosition, KeepsLegacyParagraphFallbackWhenExactOffsetIsUnavailable) {
  const auto epub = makeEpub(NESTED_CHAPTER);
  auto position = positionAt(8);
  position.hasVisibleTextOffset = false;
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, position).xpath, "/body/DocFragment[1]/body/div[1]/p[1]");
  position.hasVisibleTextOffset = true;
  position.visibleTextOffset = 10000;
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, position).xpath, "/body/DocFragment[1]/body/div[1]/p[1]");
}

TEST(KOReaderPosition, KeepsProgressFallbackWhenNeitherExactOffsetNorParagraphIsAvailable) {
  const auto epub = makeEpub("<html><body><p>abcdefghij</p></body></html>");
  auto position = positionAt(1000);
  position.pageNumber = 1;
  position.totalPages = 3;
  position.hasParagraphIndex = false;
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, position).xpath, "/body/DocFragment[1]/body/p[1]/text()[1].5");
  position.hasVisibleTextOffset = false;
  EXPECT_EQ(ProgressMapper::toSavedProgress(epub, position).xpath, "/body/DocFragment[1]/body/p[1]/text()[1].5");
}

TEST(KOReaderPosition, FailedXmlMappingRetainsLegacyFallback) {
  const auto epub = makeEpub("<html><body><p>&unknown;broken</p></body></html>");
  auto position = positionAt(1000);
  const auto saved = ProgressMapper::toSavedProgress(epub, position);
  position.hasVisibleTextOffset = false;
  const auto fallback = ProgressMapper::toSavedProgress(epub, position);
  EXPECT_FALSE(saved.xpath.empty());
  EXPECT_EQ(saved.xpath, fallback.xpath);
  EXPECT_FLOAT_EQ(saved.percentage, fallback.percentage);
}

TEST(KOReaderPosition, ExactAnchorKeepsTheExistingGlobalPercentage) {
  const auto epub = std::make_shared<Epub>(std::vector<std::string>{NESTED_CHAPTER, NESTED_CHAPTER});
  auto position = positionAt(8);
  position.spineIndex = 1;
  const auto exact = ProgressMapper::toSavedProgress(epub, position);
  position.hasVisibleTextOffset = false;
  const auto fallback = ProgressMapper::toSavedProgress(epub, position);
  EXPECT_FLOAT_EQ(exact.percentage, fallback.percentage);
  EXPECT_EQ(exact.xpath, "/body/DocFragment[2]/body/div[1]/p[1]/em[1]/text()[1].2");
}

TEST(KOReaderPosition, ExportedAnchorsRoundTripThroughTheExistingImporter) {
  const auto epub = makeEpub(NESTED_CHAPTER, 1);
  GfxRenderer renderer;
  for (uint32_t offset = 0; offset <= 16; ++offset) {
    SCOPED_TRACE(offset);
    const auto saved = ProgressMapper::toSavedProgress(epub, positionAt(offset));
    const auto restored = ProgressMapper::toCrossPoint(epub, saved, renderer);
    ASSERT_TRUE(restored.hasVisibleTextOffset);
    EXPECT_EQ(restored.spineIndex, 0);
    EXPECT_EQ(restored.visibleTextOffset, offset);
  }
}

TEST(KOReaderPosition, ExactResolverRejectsInvalidSpinesAndOutOfRangeOffsets) {
  const auto epub = makeEpub("<html><body><p>abc</p></body></html>");
  EXPECT_TRUE(ChapterXPathResolver::findXPathForVisibleTextOffset(nullptr, 0, 0).empty());
  EXPECT_TRUE(ChapterXPathResolver::findXPathForVisibleTextOffset(epub, -1, 0).empty());
  EXPECT_TRUE(ChapterXPathResolver::findXPathForVisibleTextOffset(epub, 1, 0).empty());
  EXPECT_TRUE(ChapterXPathResolver::findXPathForVisibleTextOffset(epub, 0, 4).empty());
}

TEST(KOReaderPosition, ExactResolverLeavesEmptyAndMalformedChaptersToFallbacks) {
  const auto empty = makeEpub("<html><body><img src='cover.png'/></body></html>");
  EXPECT_TRUE(ChapterXPathResolver::findXPathForVisibleTextOffset(empty, 0, 0).empty());
  const auto malformed = makeEpub("<html><body><p>&unknown;broken</p></body></html>");
  EXPECT_TRUE(ChapterXPathResolver::findXPathForVisibleTextOffset(malformed, 0, 1).empty());
}

TEST(KOReaderPosition, HiddenNestingCannotWrapTheVisibleDepthCounter) {
  std::string chapter = "<html><body><script>";
  for (int depth = 0; depth < 260; ++depth) chapter += "<span>";
  chapter += "hidden";
  for (int depth = 0; depth < 260; ++depth) chapter += "</span>";
  chapter += "</script><p>visible</p></body></html>";
  EXPECT_EQ(ChapterXPathResolver::findXPathForVisibleTextOffset(makeEpub(chapter, 1), 0, 0),
            "/body/DocFragment[1]/body/p[1]/text()[1].0");
}

}  // namespace
