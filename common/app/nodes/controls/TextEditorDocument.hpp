#ifndef LOKA_APP_TEXT_EDITOR_DOCUMENT_HPP
#define LOKA_APP_TEXT_EDITOR_DOCUMENT_HPP
#include "app/style/LineCursor.hpp"
#include "core/String.hpp"
namespace loka
{
  namespace app
  {
    struct TextEditorProps;
    /** Refusal loses only the current action; committed text and cursor survive. */
    enum EditorResult
    {
      EDITOR_OK,
      EDITOR_CAPACITY,
      EDITOR_STALE_ID,
      EDITOR_ALLOCATION,
      EDITOR_ID_EXHAUSTED,
      EDITOR_REENTRANT,
      EDITOR_UNAVAILABLE,
      EDITOR_NON_ASCII,
      EDITOR_INVALID_CURSOR,
      EDITOR_OWNER_MISMATCH
    };
    /** Node-owned edit initiator, borrowing current Props. The app owns both facts.
        All commit doors validate before mutation and group cursor and list writes
        in the list owner's transaction. No callback or platform resource is owned. */
    class TextEditorDocument
    {
    public:
      explicit TextEditorDocument(const TextEditorProps &props)
          : props_(props)
      {
      }
      EditorResult availability() const;
      EditorResult applySingleLine(core::ItemId id, const core::String &text, LineCursor after);
      EditorResult applySplit(core::ItemId id, LineCursor::Column column);
      EditorResult applyJoin(core::ItemId id);
      /** Insert CR/LF/CRLF-normalized text at a caret in one operation. */
      EditorResult applyKeystroke(LineCursor before, const char *bytes, std::size_t length);
      EditorResult moveCaret(LineCursor after);
      /** Bounded serialization for projections. No publication. */
      EditorResult project(std::string &out) const;
      /** Serialize into caller-reserved storage; never grow the destination. */
      EditorResult project(char *out, std::size_t capacity, std::size_t &length) const;

    private:
      TextEditorDocument(const TextEditorDocument &);
      TextEditorDocument &operator=(const TextEditorDocument &);
      EditorResult measure(std::size_t &bytes) const;
      EditorResult validateCursor(LineCursor cursor) const;
      EditorResult
      commit(core::ListOp<core::String> *ops, unsigned short count, LineCursor after, int insertedIndex = -1);
      const TextEditorProps &props_;
    };
  } // namespace app
} // namespace loka
#endif
