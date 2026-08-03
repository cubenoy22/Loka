#include "NullTextLayoutTests.hpp"

#include <cassert>

#include "app/nodes/Text.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/context/NullTextContext.hpp"

void testNullTextLayoutWordAndCharacterWrapProduceDifferentGeometry()
{
  loka::app::TextProps noneProps("aa bbbb cc");
  noneProps.attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_NONE));
  loka::app::TextNode noneText(noneProps);
  loka::app::TextProps wordProps("aa bbbb cc");
  wordProps.attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD));
  loka::app::TextNode wordText(wordProps);
  loka::app::TextProps characterProps("aa bbbb cc");
  characterProps.attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_CHAR));
  loka::app::TextNode characterText(characterProps);
  loka::app::scene::LayoutState state;
  state.y = 5;
  state.width = 24;
  state.lineHeight = 10;
  NullScenePlatformController platform;

  const int noneResultY = platform.projectLayoutForTesting(&noneText, state);
  const int wordResultY = platform.projectLayoutForTesting(&wordText, state);
  const int characterResultY = platform.projectLayoutForTesting(&characterText, state);

  (void)noneResultY;
  (void)wordResultY;
  assert(noneResultY != wordResultY);
  (void)characterResultY;
  assert(noneResultY != characterResultY);
  assert(wordResultY != characterResultY);
  assert(noneResultY == 15);
  assert(wordResultY == 35);
  assert(characterResultY == 25);
}

namespace
{
  const NullTextMeasurement &measurementFor(loka::app::TextNode &text)
  {
    NullTextContext *context = static_cast<NullTextContext *>(text.getContext());
    assert(context);
    return context->measurement();
  }
} // namespace

void testNullTextLayoutTruncationModesProduceDifferentWidths()
{
  (void)&measurementFor;
  loka::app::TextProps noneProps("abcdefghij");
  noneProps.attr(loka::app::TextAttr().truncation(loka::app::TEXT_TRUNCATION_NONE));
  loka::app::TextNode noneText(noneProps);
  loka::app::TextProps clipProps("abcdefghij");
  clipProps.attr(loka::app::TextAttr().truncation(loka::app::TEXT_TRUNCATION_CLIP));
  loka::app::TextNode clipText(clipProps);
  loka::app::TextProps ellipsisProps("abcdefghij");
  ellipsisProps.attr(loka::app::TextAttr().truncation(loka::app::TEXT_TRUNCATION_ELLIPSIS));
  loka::app::TextNode ellipsisText(ellipsisProps);
  loka::app::scene::LayoutState state;
  state.width = 22;
  state.lineHeight = 10;
  NullScenePlatformController platform;

  platform.projectLayoutForTesting(&noneText, state);
  platform.projectLayoutForTesting(&clipText, state);
  platform.projectLayoutForTesting(&ellipsisText, state);

  assert(measurementFor(noneText).width() == 40);
  assert(measurementFor(clipText).width() == 22);
  assert(measurementFor(ellipsisText).width() == 20);
}

void testNullTextLayoutHonorsExplicitBreaksAndForceBreaksLongWords()
{
  loka::app::TextProps breakProps("a\nb");
  breakProps.attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD));
  loka::app::TextNode breakText(breakProps);
  loka::app::TextProps longWordProps("abcdefgh");
  longWordProps.attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD));
  loka::app::TextNode longWordText(longWordProps);
  loka::app::scene::LayoutState state;
  state.width = 16;
  state.lineHeight = 10;
  NullScenePlatformController platform;

  const int breakResultY = platform.projectLayoutForTesting(&breakText, state);
  const int longWordResultY = platform.projectLayoutForTesting(&longWordText, state);

  (void)breakResultY;
  assert(breakResultY == 20);
  assert(measurementFor(breakText).width() == 4);
  assert(measurementFor(breakText).lineCount() == 2);
  (void)longWordResultY;
  assert(longWordResultY == 20);
  assert(measurementFor(longWordText).width() == 16);
  assert(measurementFor(longWordText).lineCount() == 2);
}

void testNullTextLayoutUsesFixedAdvancePerCodePoint()
{
  const char utf8[] = "\xC3\xA9\xC3\xA9\xC3\xA9";
  loka::app::TextProps props(loka::core::String::Utf8(utf8, 6));
  loka::app::TextNode text(props);
  loka::app::scene::LayoutState state;
  state.width = 100;
  NullScenePlatformController platform;

  platform.projectLayoutForTesting(&text, state);

  assert(measurementFor(text).width() == 12);
  assert(measurementFor(text).lineCount() == 1);
}

void testNullTextLayoutPreservesNegativeStartY()
{
  loka::app::TextNode text((loka::app::TextProps("a")));
  loka::app::scene::LayoutState state;
  state.y = -20;
  state.width = 100;
  state.lineHeight = 10;
  NullScenePlatformController platform;

  const int resultY = platform.projectLayoutForTesting(&text, state);

  (void)resultY;
  assert(resultY == -10);
  assert(measurementFor(text).height() == 10);
}

void testNullTextLayoutWrapsAtPositiveSubGlyphWidth()
{
  loka::app::TextProps wordProps("ab");
  wordProps.attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD));
  loka::app::TextNode wordText(wordProps);
  loka::app::TextProps characterProps("ab");
  characterProps.attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_CHAR));
  loka::app::TextNode characterText(characterProps);
  loka::app::scene::LayoutState state;
  state.width = 2;
  state.lineHeight = 10;
  NullScenePlatformController platform;

  const int wordResultY = platform.projectLayoutForTesting(&wordText, state);
  const int characterResultY = platform.projectLayoutForTesting(&characterText, state);

  (void)wordResultY;
  assert(wordResultY == 20);
  (void)characterResultY;
  assert(characterResultY == 20);
  assert(measurementFor(wordText).width() == 4);
  assert(measurementFor(characterText).width() == 4);
  assert(measurementFor(wordText).lineCount() == 2);
  assert(measurementFor(characterText).lineCount() == 2);
}

void testNullTextLayoutWordWrapMeasuresStandaloneSpaces()
{
  loka::app::TextProps props("    ");
  props.attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD));
  loka::app::TextNode text(props);
  loka::app::scene::LayoutState state;
  state.width = 8;
  state.lineHeight = 10;
  NullScenePlatformController platform;

  const int resultY = platform.projectLayoutForTesting(&text, state);

  (void)resultY;
  assert(resultY == 20);
  assert(measurementFor(text).width() == 8);
  assert(measurementFor(text).lineCount() == 2);
}
