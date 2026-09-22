#ifndef LOKA_TEST_TEXT_EDITOR_ACCESS_HPP
#define LOKA_TEST_TEXT_EDITOR_ACCESS_HPP
#ifndef TEST_BUILD
#error TextEditorAccess is test-only
#endif
#include "app/nodes/controls/TextEditor.hpp"
namespace loka
{
  namespace app
  {
    namespace testing
    {
      /** Fixture access to the node-owned commit seam. */
      class TextEditorAccess
      {
      public:
        static TextEditorDocument &document(TextEditorNode &node)
        {
          return node.document;
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka
#endif
