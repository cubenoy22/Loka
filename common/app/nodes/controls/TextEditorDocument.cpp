#include "app/nodes/controls/TextEditor.hpp"
#include "platform/String.hpp"
#include "core/StringAccess.hpp"
#include <cstring>

namespace loka
{
  namespace app
  {
    namespace
    {
      /** Borrow the rail's UTF-8 bytes when available. Other rails retain the
          existing StringBuffer conversion/refusal contract, as AttributedString
          does. String construction retains the rail contract (#827). */
      class LineBytes
      {
      public:
        explicit LineBytes(const core::String &value)
            : buffer_(),
              view_(),
              result_(EDITOR_OK)
        {
          const core::Managed<platform::String> &handle = core::StringAccess::handle(value);
          if (!handle.isValid())
          {
            this->view_.bytes = "";
            this->view_.length = 0;
            return;
          }
          if (!handle->queryUtf8(this->view_))
          {
            this->buffer_ = value.bufferWithEncoding(core::StringEncodingUtf8);
            if (!this->buffer_.platformHandle().isValid())
            {
              this->result_ = EDITOR_ALLOCATION;
              return;
            }
            this->view_.bytes = static_cast<const char *>(this->buffer_.data());
            this->view_.length = this->buffer_.length();
          }
          if (!this->view_.length)
            this->view_.bytes = "";
          if (this->view_.length > TextEditorProps::kMaxBytes)
          {
            this->result_ = EDITOR_CAPACITY;
            return;
          }
          for (std::size_t i = 0; i < this->view_.length; ++i)
          {
            const unsigned char c = static_cast<unsigned char>(this->view_.bytes[i]);
            if (!c || c > 127 || c == '\r' || c == '\n')
            {
              this->result_ = EDITOR_NON_ASCII;
              return;
            }
          }
        }
        EditorResult result() const
        {
          return this->result_;
        }
        const char *data() const
        {
          return this->view_.bytes;
        }
        std::size_t size() const
        {
          return this->view_.length;
        }

      private:
        LineBytes(const LineBytes &);
        LineBytes &operator=(const LineBytes &);
        core::StringBuffer buffer_;
        platform::Utf8View view_;
        EditorResult result_;
      };
      /** Local checked construction storage. No retained draft or owner state. */
      class EditorScratch
      {
      public:
        explicit EditorScratch(std::size_t size)
            : data_(static_cast<char *>(
                  core::LokaAllocRaw(size ? size : 1, core::LokaAllocationSite("TextEditor", "Scratch"))))
        {
        }
        ~EditorScratch()
        {
          core::LokaFreeRaw(this->data_, core::LokaAllocationSite("TextEditor", "Scratch"));
        }
        char *data() const
        {
          return this->data_;
        }

      private:
        EditorScratch(const EditorScratch &);
        EditorScratch &operator=(const EditorScratch &);
        char *data_;
      };
      EditorResult resultOf(core::ListEditResult result)
      {
        switch (result)
        {
        case core::EDIT_OK:
          return EDITOR_OK;
        case core::EDIT_CAPACITY_EXCEEDED:
          return EDITOR_CAPACITY;
        case core::EDIT_ID_NOT_FOUND:
          return EDITOR_STALE_ID;
        case core::EDIT_INDEX_OUT_OF_RANGE:
          return EDITOR_INVALID_CURSOR;
        case core::EDIT_SEQ_EXHAUSTED:
        case core::EDIT_GENERATION_EXHAUSTED:
          return EDITOR_ID_EXHAUSTED;
        case core::EDIT_NOT_ATTACHED:
          return EDITOR_UNAVAILABLE;
        case core::EDIT_REENTRANT:
          return EDITOR_REENTRANT;
        }
        return EDITOR_UNAVAILABLE;
      }
    } // namespace
    EditorResult TextEditorDocument::measure(std::size_t &bytes) const
    {
      bytes = 0;
      const core::ObservableList<core::String> *lines = this->props_.lines_;
      if (!lines || !this->props_.cursor_.isValid())
        return EDITOR_UNAVAILABLE;
      core::StateTracker *owner = 0;
      const core::ListEditResult ready = lines->queryMutationTracker(owner);
      if (ready != core::EDIT_OK && ready != core::EDIT_REENTRANT)
        return resultOf(ready);
      if (!this->props_.cursor_.usesTracker(owner))
        return EDITOR_OWNER_MISMATCH;
      if (lines->size() > TextEditorProps::kMaxLines)
        return EDITOR_CAPACITY;
      for (unsigned short i = 0; i < lines->size(); ++i)
      {
        const LineBytes line(lines->at(i).value);
        if (line.result() != EDITOR_OK)
          return line.result();
        bytes += line.size() + (i ? 1 : 0);
        if (bytes > TextEditorProps::kMaxBytes)
          return EDITOR_CAPACITY;
      }
      return EDITOR_OK;
    }
    EditorResult TextEditorDocument::project(std::string &out) const
    {
      out.clear();
      std::size_t bytes = 0;
      EditorResult result = this->measure(bytes);
      if (result != EDITOR_OK)
        return result;
      for (unsigned short i = 0; i < this->props_.lines_->size(); ++i)
      {
        const LineBytes line(this->props_.lines_->at(i).value);
        if (line.result() != EDITOR_OK)
        {
          out.clear();
          return line.result();
        }
        if (i)
          out += '\r';
        out.append(line.data(), line.size());
      }
      return EDITOR_OK;
    }
    EditorResult TextEditorDocument::project(char *out, std::size_t capacity, std::size_t &length) const
    {
      length = 0;
      std::size_t bytes = 0;
      EditorResult result = this->measure(bytes);
      if (result != EDITOR_OK)
        return result;
      if (!out || bytes > capacity)
        return EDITOR_ALLOCATION;
      for (unsigned short i = 0; i < this->props_.lines_->size(); ++i)
      {
        const LineBytes line(this->props_.lines_->at(i).value);
        if (line.result() != EDITOR_OK)
          return line.result();
        if (i)
          out[length++] = '\r';
        std::memcpy(out + length, line.data(), line.size());
        length += line.size();
      }
      return EDITOR_OK;
    }
    EditorResult TextEditorDocument::availability() const
    {
      std::size_t bytes = 0;
      return this->measure(bytes);
    }
    EditorResult TextEditorDocument::validateCursor(LineCursor cursor) const
    {
      if (cursor.isNone())
        return EDITOR_OK;
      const int index = this->props_.lines_->find(cursor.line);
      if (index < 0)
        return EDITOR_STALE_ID;
      const LineBytes line(this->props_.lines_->at(static_cast<unsigned short>(index)).value);
      if (line.result() != EDITOR_OK)
        return line.result();
      return cursor.column >= 0 && static_cast<std::size_t>(cursor.column) <= line.size() ? EDITOR_OK
                                                                                          : EDITOR_INVALID_CURSOR;
    }
    EditorResult TextEditorDocument::commit(core::ListOp<core::String> *ops,
                                            unsigned short count,
                                            LineCursor after,
                                            int insertedIndex)
    {
      core::ObservableList<core::String> &lines = *this->props_.lines_;
      const scene::WriteSeat<LineCursor> cursor = this->props_.cursor_;
      core::StateTracker *owner = 0;
      const core::ListEditResult ready = lines.queryMutationTracker(owner);
      if (ready != core::EDIT_OK)
        return resultOf(ready);
      core::StateTrackerGuard guard(owner);
      core::ArrayListOpCursor<core::String> batch(ops, count);
      const core::ListEditResult edited =
          count == 1 && ops[0].kind == core::UPDATE ? lines.update(ops[0].id, ops[0].after) : lines.apply(batch);
      if (edited != core::EDIT_OK)
        return resultOf(edited);
      if (insertedIndex >= 0)
        after.line = lines.at(static_cast<unsigned short>(insertedIndex)).id;
      cursor.set(after);
      return EDITOR_OK;
    }
    EditorResult TextEditorDocument::applySingleLine(core::ItemId id, const core::String &text, LineCursor after)
    {
      std::size_t total = 0;
      EditorResult result = this->measure(total);
      if (result != EDITOR_OK)
        return result;
      const int index = this->props_.lines_->find(id);
      if (index < 0)
        return EDITOR_STALE_ID;
      const LineBytes replacement(text), old(this->props_.lines_->at(static_cast<unsigned short>(index)).value);
      if (replacement.result() != EDITOR_OK)
        return replacement.result();
      if (old.result() != EDITOR_OK)
        return old.result();
      if (total - old.size() + replacement.size() > TextEditorProps::kMaxBytes)
        return EDITOR_CAPACITY;
      if (after.line == id)
      {
        if (after.column < 0 || static_cast<std::size_t>(after.column) > replacement.size())
          return EDITOR_INVALID_CURSOR;
      }
      else if ((result = this->validateCursor(after)) != EDITOR_OK)
        return result;
      core::ListOp<core::String> op(core::UPDATE, id, 0, text);
      return this->commit(&op, 1, after);
    }
    EditorResult TextEditorDocument::applyKeystroke(LineCursor before, const char *bytes, std::size_t length)
    {
      std::size_t total = 0;
      EditorResult result = this->measure(total);
      if (result != EDITOR_OK)
        return result;
      if (before.isNone())
        return EDITOR_INVALID_CURSOR;
      result = this->validateCursor(before);
      if (result != EDITOR_OK)
        return result;
      if (length > 2 * TextEditorProps::kMaxBytes)
        return EDITOR_CAPACITY;
      if (!bytes && length)
        return EDITOR_NON_ASCII;
      unsigned short breaks = 0;
      std::size_t normalized = 0, lastColumn = 0;
      for (std::size_t i = 0; i < length; ++i)
      {
        const unsigned char c = static_cast<unsigned char>(bytes[i]);
        if (!c || c > 127)
          return EDITOR_NON_ASCII;
        ++normalized;
        if (c == '\r' || c == '\n')
        {
          ++breaks;
          lastColumn = 0;
          if (c == '\r' && i + 1 < length && bytes[i + 1] == '\n')
            ++i;
        }
        else
          ++lastColumn;
      }
      core::ObservableList<core::String> &lines = *this->props_.lines_;
      if (total + normalized > TextEditorProps::kMaxBytes || lines.size() + breaks > TextEditorProps::kMaxLines
          || lines.size() + breaks > lines.capacity())
        return EDITOR_CAPACITY;
      const unsigned short index = static_cast<unsigned short>(lines.find(before.line));
      const LineBytes old(lines.at(index).value);
      if (old.result() != EDITOR_OK)
        return old.result();
      const std::size_t size = old.size() + normalized;
      EditorScratch scratch(size);
      if (!scratch.data())
        return EDITOR_ALLOCATION;
      std::memcpy(scratch.data(), old.data(), before.column);
      std::size_t position = static_cast<std::size_t>(before.column);
      for (std::size_t i = 0; i < length; ++i)
      {
        const char c = bytes[i];
        scratch.data()[position++] = c == '\n' ? '\r' : c;
        if (c == '\r' && i + 1 < length && bytes[i + 1] == '\n')
          ++i;
      }
      std::memcpy(scratch.data() + position, old.data() + before.column, old.size() - before.column);
      core::ListOp<core::String> ops[TextEditorProps::kMaxLines];
      std::size_t start = 0;
      for (unsigned short i = 0; i <= breaks; ++i)
      {
        std::size_t stop = start;
        while (stop < size && scratch.data()[stop] != '\r')
          ++stop;
        const core::String text = core::String::Utf8(scratch.data() + start, stop - start);
        ops[i] = core::ListOp<core::String>(i ? core::INSERT : core::UPDATE,
                                            i ? core::ItemId() : before.line,
                                            static_cast<unsigned short>(index + i),
                                            text);
        start = stop + 1;
      }
      const LineCursor::Column column = breaks ? static_cast<LineCursor::Column>(lastColumn)
                                               : before.column + static_cast<LineCursor::Column>(normalized);
      return this->commit(
          ops, static_cast<unsigned short>(breaks + 1), LineCursor(before.line, column), breaks ? index + breaks : -1);
    }
    EditorResult TextEditorDocument::applySplit(core::ItemId id, LineCursor::Column column)
    {
      return this->applyKeystroke(LineCursor(id, column), "\r", 1);
    }
    EditorResult TextEditorDocument::applyJoin(core::ItemId id)
    {
      EditorResult result = this->availability();
      if (result != EDITOR_OK)
        return result;
      core::ObservableList<core::String> &lines = *this->props_.lines_;
      const int index = lines.find(id);
      if (index < 0)
        return EDITOR_STALE_ID;
      if (!index)
        return EDITOR_INVALID_CURSOR;
      const LineBytes prefix(lines.at(static_cast<unsigned short>(index - 1)).value),
          suffix(lines.at(static_cast<unsigned short>(index)).value);
      if (prefix.result() != EDITOR_OK)
        return prefix.result();
      if (suffix.result() != EDITOR_OK)
        return suffix.result();
      const LineCursor after(lines.at(static_cast<unsigned short>(index - 1)).id,
                             static_cast<LineCursor::Column>(prefix.size()));
      EditorScratch scratch(prefix.size() + suffix.size());
      if (!scratch.data())
        return EDITOR_ALLOCATION;
      std::memcpy(scratch.data(), prefix.data(), prefix.size());
      std::memcpy(scratch.data() + prefix.size(), suffix.data(), suffix.size());
      const core::String text = core::String::Utf8(scratch.data(), prefix.size() + suffix.size());
      core::ListOp<core::String> ops[2] = {core::ListOp<core::String>(core::UPDATE, after.line, 0, text),
                                           core::ListOp<core::String>(core::REMOVE, id)};
      return this->commit(ops, 2, after);
    }
    EditorResult TextEditorDocument::moveCaret(LineCursor after)
    {
      if (!this->props_.lines_)
        return EDITOR_UNAVAILABLE;
      core::StateTracker *owner = 0;
      const core::ListEditResult ready = this->props_.lines_->queryMutationTracker(owner);
      if (ready != core::EDIT_OK)
        return resultOf(ready);
      EditorResult result = this->availability();
      if (result != EDITOR_OK)
        return result;
      result = this->validateCursor(after);
      if (result != EDITOR_OK)
        return result;
      core::StateTrackerGuard guard(owner);
      this->props_.cursor_.set(after);
      return EDITOR_OK;
    }
  } // namespace app
} // namespace loka
