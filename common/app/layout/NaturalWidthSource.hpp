#ifndef LOKA_APP_LAYOUT_NATURAL_WIDTH_SOURCE_HPP
#define LOKA_APP_LAYOUT_NATURAL_WIDTH_SOURCE_HPP

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Node;
    }
    namespace layout
    {
      /** Borrowed rail measurement for an undeclared-width Row child.
          Answers (including refusal) must remain stable throughout one synchronous
          layout pass: no state writes, event pumping or consuming measurement tokens.
          A successful answer is positive. The Row may ask twice, while totalling
          and allocating; the source and its inputs must outlive that pass. */
      class INaturalWidthSource
      {
      public:
        virtual ~INaturalWidthSource() {}
        virtual bool queryNaturalWidth(scene::Node *child, short &width) const = 0;
      };
    } // namespace layout
  } // namespace app
} // namespace loka

#endif
