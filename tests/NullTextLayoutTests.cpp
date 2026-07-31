#include "NullTextLayoutTests.hpp"

#include <cassert>

#include "app/nodes/Text.hpp"
#include "platform/null/NullScenePlatformController.hpp"

void testNullTextLayoutWordAndCharacterWrapProduceDifferentGeometry()
{
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

  const int wordResultY = platform.projectLayoutForTesting(&wordText, state);
  const int characterResultY = platform.projectLayoutForTesting(&characterText, state);

  assert(wordResultY != characterResultY);
  assert(wordResultY == 35);
  assert(characterResultY == 25);
}
