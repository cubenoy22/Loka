#ifndef LOKA_TEST_TEXT_EDITOR_CONTRACT_SNAPSHOT_HPP
#define LOKA_TEST_TEXT_EDITOR_CONTRACT_SNAPSHOT_HPP
#include "app/nodes/controls/TextEditor.hpp"
#include "support/TestVerify.hpp"
#include <vector>
namespace loka
{
  namespace testing
  {
    /** The same refusal invariant for Null and native host rails. Fixture supplies
        app-owned lines/cursor; native projection assertions remain rail-specific. */
    class TextEditorContractSnapshot
    {
    public:
      std::vector<core::ItemId> ids;
      std::vector<core::String> text;
      core::ListRevision revision;
      app::LineCursor cursor;
      template <class Fixture>
      explicit TextEditorContractSnapshot(const Fixture &f)
          : revision(f.lines.revision().get()),
            cursor(f.cursor.state()->get())
      {
        for (unsigned short i = 0; i < f.lines.size(); ++i)
        {
          this->ids.push_back(f.lines.at(i).id);
          this->text.push_back(f.lines.at(i).value);
        }
      }
      template <class Fixture> void unchanged(const Fixture &f) const
      {
        LOKA_VERIFY(this->ids.size() == f.lines.size());
        for (unsigned short i = 0; i < f.lines.size(); ++i)
        {
          LOKA_VERIFY(this->ids[i] == f.lines.at(i).id);
          LOKA_VERIFY(this->text[i].equals(f.lines.at(i).value));
        }
        LOKA_VERIFY(!(this->revision != f.lines.revision().get()));
        LOKA_VERIFY(this->cursor == f.cursor.state()->get());
      }
    };
  } // namespace testing
} // namespace loka
#endif
