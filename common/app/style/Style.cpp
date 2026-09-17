#include "app/style/Style.hpp"

#define LOKA_STYLE_VOCAB_DEFINE_STORAGE
#include "app/style/StyleVocab.hpp"
#undef LOKA_STYLE_VOCAB_DEFINE_STORAGE

namespace loka
{
  namespace app
  {
    TextStyle SizeOf(int logicalUnits)
    {
      int selected = detail::StyleVocabularySizes[0];
      if (logicalUnits > selected)
      {
        for (int i = 1; i < detail::StyleVocabularySizeCount; ++i)
        {
          const int candidate = detail::StyleVocabularySizes[i];
          if (logicalUnits >= candidate)
          {
            selected = candidate;
            continue;
          }
          if (logicalUnits - selected > candidate - logicalUnits)
          {
            selected = candidate;
          }
          break;
        }
      }
      TextStyle result;
      result.fontSize_ = selected;
      result.hasFontSize_ = true;
      return result;
    }

    const TextStyle Bold = TextStyle().weight(TEXT_WEIGHT_BOLD);
    const TextStyle Italic = TextStyle().italic();
    const TextStyle Body = FontSize<12>();
    const TextStyle Caption = FontSize<9>();
    const TextStyle Heading = FontSize<12>() + Bold;
    const TextStyle Title = FontSize<18>() + Bold;
  } // namespace app
} // namespace loka
