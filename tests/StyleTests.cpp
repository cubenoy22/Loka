#include "StyleTests.hpp"

#include <climits>

#include "app/nodes/Text.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/context/NullTextContext.hpp"
#include "support/TestVerify.hpp"

namespace
{
  // A namespace-scope style composed from the named styles, the shape an
  // application would write. It must be initialised after Bold and Italic,
  // which the header guarantees by defining them per translation unit.
  const loka::app::TextStyle kWarningStyle = loka::app::Bold + loka::app::Italic;
}

void testTextStyleMergeAndSizeVocabulary()
{
  using namespace loka::app;
  LOKA_VERIFY(kWarningStyle.hasWeight_ && kWarningStyle.weight_ == TEXT_WEIGHT_BOLD);
  LOKA_VERIFY(kWarningStyle.hasItalic_ && kWarningStyle.italic_);
  const TextStyle boldLarge = Bold + FontSize<24>();
  const TextStyle largeBold = FontSize<24>() + Bold;
  LOKA_VERIFY(boldLarge == largeBold);
  LOKA_VERIFY(boldLarge.hasFontSize_ && boldLarge.fontSize_ == 24);
  LOKA_VERIFY(boldLarge.hasWeight_ && boldLarge.weight_ == TEXT_WEIGHT_BOLD);

  const TextStyle replaced = FontSize<12>() + FontSize<24>();
  LOKA_VERIFY(replaced.hasFontSize_ && replaced.fontSize_ == 24);
  LOKA_VERIFY(TextStyle() + Bold == Bold);
  LOKA_VERIFY(Bold + TextStyle() == Bold);
  LOKA_VERIFY(Italic.hasItalic_ && Italic.italic_);
  LOKA_VERIFY(Body == FontSize<12>());
  LOKA_VERIFY(Caption == FontSize<9>());
  LOKA_VERIFY(Heading == FontSize<12>() + Bold);
  LOKA_VERIFY(Title == FontSize<18>() + Bold);

  const BlockStyle wrapped = BlockStyle().wrap(TEXT_WRAP_WORD);
  const BlockStyle clipped = wrapped + BlockStyle().truncation(TEXT_TRUNCATION_CLIP);
  LOKA_VERIFY(clipped.hasWrap_ && clipped.wrap_ == TEXT_WRAP_WORD);
  LOKA_VERIFY(clipped.hasTruncation_ && clipped.truncation_ == TEXT_TRUNCATION_CLIP);
  LOKA_VERIFY(clipped + BlockStyle() == clipped);
}

void testSizeOfSnapsToNearestVocabularySizeWithTiesDown()
{
  using namespace loka::app;
  const int inputs[] = {INT_MIN, 0, 9, 10, 11, 12, 13, 16, 18, 21, 24, 99, INT_MAX};
  const int expected[] = {9, 9, 9, 10, 10, 12, 12, 14, 18, 18, 24, 24, 24};
  const int count = static_cast<int>(sizeof(inputs) / sizeof(inputs[0]));
  for (int i = 0; i < count; ++i)
  {
    const TextStyle style = SizeOf(inputs[i]);
    LOKA_VERIFY(style.hasFontSize_);
    LOKA_VERIFY(style.fontSize_ == expected[i]);
  }
}

void testTextPropsItalicParticipatesInDefinitionEquivalence()
{
  using namespace loka::app;
  const TextDefinition plain = Text("same");
  const TextDefinitionWithAttr italic = Text("same") + Italic;
  LOKA_VERIFY(!plain.hasEquivalentProps(italic));
  LOKA_VERIFY(!italic.hasEquivalentProps(plain));
}

namespace
{
  short layoutHeight(const loka::app::TextDefinitionWithAttr &definition)
  {
    loka::app::TextNode node(definition.props);
    loka::app::scene::LayoutState state;
    state.width = 100;
    NullScenePlatformController platform;
    (void)platform.projectLayoutForTesting(&node, state);
    NullTextContext *context = static_cast<NullTextContext *>(node.getContext());
    LOKA_VERIFY(context != 0);
    return context->measurement().height();
  }
}

void testNullTextLayoutUsesResolvedFontSize()
{
  const short small = layoutHeight(loka::app::Text("x") + loka::app::FontSize<12>());
  const short large = layoutHeight(loka::app::Text("x") + loka::app::FontSize<24>());
  loka::core::MutableState<loka::app::TextStyle> live((loka::app::FontSize<24>()));
  const short liveWins = layoutHeight((loka::app::Text("x") + loka::app::FontSize<12>()) + &live);
  LOKA_VERIFY(small == 12);
  LOKA_VERIFY(large == 24);
  LOKA_VERIFY(liveWins == 24);
  LOKA_VERIFY(small != large);
}

void testTextStyleDslPreservesDefinitionPolicies()
{
  using namespace loka::app;
  TextDefinition source("x");
  source.testId("StyledText");
  source.setNodeTag(41);
  source.setCompositionSeatSlot(7);
  source.setNativeLifetimeHint(scene::NATIVE_HINT_DESIRE_STAY);
  const TextDefinitionWithAttr result = source + Bold;
  LOKA_VERIFY(result.hasTestId());
  LOKA_VERIFY(result.testIdValue() == "StyledText");
  LOKA_VERIFY(result.nodeTag() == 41);
  LOKA_VERIFY(result.compositionSeatSlot() == 7);
  LOKA_VERIFY(result.nativeLifetimeHint() == scene::NATIVE_HINT_DESIRE_STAY);
}
