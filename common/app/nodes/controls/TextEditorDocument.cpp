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
      /** Stack-borrowed replay: live IDs remain stable until apply swaps its buffers.
          Payload construction is complete before either allocation-free traversal. */
      class ReplaceOpCursor : public core::ListOpCursor<core::String>
      {
      public:
        ReplaceOpCursor(const core::ObservableList<core::String> &lines,
                        unsigned short first,
                        unsigned short last,
                        const core::String *payloads,
                        unsigned short breaks)
            : lines_(lines),
              first_(first),
              last_(last),
              payloads_(payloads),
              breaks_(breaks),
              index_(0)
        {
        }
        virtual void rewind()
        {
          this->index_ = 0;
        }
        virtual bool next(core::ListOp<core::String> &out)
        {
          const unsigned short removed = this->last_ - this->first_;
          if (this->index_ > removed + this->breaks_)
            return false;
          if (!this->index_)
            out = core::ListOp<core::String>(core::UPDATE, this->lines_.at(this->first_).id, 0, this->payloads_[0]);
          else if (this->index_ <= removed)
            out = core::ListOp<core::String>(core::REMOVE, this->lines_.at(this->first_ + this->index_).id);
          else
          {
            const unsigned short inserted = this->index_ - removed;
            out = core::ListOp<core::String>(
                core::INSERT, core::ItemId(), this->first_ + inserted, this->payloads_[inserted]);
          }
          ++this->index_;
          return true;
        }

      private:
        const core::ObservableList<core::String> &lines_;
        const unsigned short first_, last_;
        const core::String *const payloads_;
        const unsigned short breaks_;
        unsigned short index_;
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
    EditorResult TextEditorDocument::commit(core::ListOpCursor<core::String> &ops, RowCursor after)
    {
      core::ObservableList<core::String> &lines = *this->props_.lines_;
      const scene::WriteSeat<LineCursor> cursor = this->props_.cursor_;
      core::StateTracker *owner = 0;
      const core::ListEditResult ready = lines.queryMutationTracker(owner);
      if (ready != core::EDIT_OK)
        return resultOf(ready);
      core::StateTrackerGuard guard(owner);
      // A lone UPDATE (the ordinary keystroke) keeps the in-place update path:
      // apply() copies the whole list twice, which the 68k keystroke budget cannot spare.
      core::ListOp<core::String> only, more;
      ops.rewind();
      const bool single = ops.next(only) && only.kind == core::UPDATE && !ops.next(more);
      const core::ListEditResult edited = single ? lines.update(only.id, only.after) : lines.apply(ops);
      if (edited != core::EDIT_OK)
        return resultOf(edited);
      cursor.set(after.isNone() ? LineCursor::None() : LineCursor(lines.at(after.row).id, after.column));
      return EDITOR_OK;
    }
    EditorResult TextEditorDocument::applyReplace(LineCursor from, LineCursor to, const char *bytes, std::size_t length)
    {
      return this->replace(from, to, bytes, length, 0);
    }
    EditorResult TextEditorDocument::applyReplace(
        LineCursor from, LineCursor to, const char *bytes, std::size_t length, RowCursor after)
    {
      return this->replace(from, to, bytes, length, &after);
    }
    EditorResult TextEditorDocument::replace(
        LineCursor from, LineCursor to, const char *bytes, std::size_t length, const RowCursor *after)
    {
      std::size_t total = 0;
      EditorResult result = this->measure(total);
      if (result != EDITOR_OK)
        return result;
      if (from.isNone() || to.isNone())
        return EDITOR_INVALID_CURSOR;
      result = this->validateCursor(from);
      if (result != EDITOR_OK)
        return result;
      result = this->validateCursor(to);
      if (result != EDITOR_OK)
        return result;
      core::ObservableList<core::String> &lines = *this->props_.lines_;
      const unsigned short first = static_cast<unsigned short>(lines.find(from.line));
      const unsigned short last = static_cast<unsigned short>(lines.find(to.line));
      if (first > last || (first == last && from.column > to.column))
        return EDITOR_INVALID_CURSOR;
      core::StateTracker *owner = 0;
      const core::ListEditResult ready = lines.queryMutationTracker(owner);
      if (ready != core::EDIT_OK)
        return resultOf(ready);
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
      if (from == to && !length)
      {
        if (!after)
          return EDITOR_OK;
        if (after->isNone())
          return this->moveCaret(LineCursor::None());
        if (after->row >= lines.size())
          return EDITOR_INVALID_CURSOR;
        return this->moveCaret(LineCursor(lines.at(after->row).id, after->column));
      }
      const LineBytes prefix(lines.at(first).value), suffix(lines.at(last).value);
      if (prefix.result() != EDITOR_OK)
        return prefix.result();
      if (suffix.result() != EDITOR_OK)
        return suffix.result();
      std::size_t removed = to.column - from.column;
      if (first != last)
      {
        removed = prefix.size() - from.column + to.column + (last - first);
        for (unsigned short i = first + 1; i < last; ++i)
        {
          const LineBytes middle(lines.at(i).value);
          if (middle.result() != EDITOR_OK)
            return middle.result();
          removed += middle.size();
        }
      }
      const unsigned short finalCount = lines.size() - (last - first) + breaks;
      if (total - removed + normalized > TextEditorProps::kMaxBytes || finalCount > TextEditorProps::kMaxLines
          || finalCount > lines.capacity())
        return EDITOR_CAPACITY;
      const std::size_t size = from.column + normalized + suffix.size() - to.column;
      EditorScratch scratch(size);
      if (!scratch.data())
        return EDITOR_ALLOCATION;
      std::memcpy(scratch.data(), prefix.data(), from.column);
      std::size_t position = static_cast<std::size_t>(from.column);
      for (std::size_t i = 0; i < length; ++i)
      {
        const char c = bytes[i];
        scratch.data()[position++] = c == '\n' ? '\r' : c;
        if (c == '\r' && i + 1 < length && bytes[i + 1] == '\n')
          ++i;
      }
      std::memcpy(scratch.data() + position, suffix.data() + to.column, suffix.size() - to.column);
      // 68k sizeof(String[kMaxLines]) = 1,024 B; former ListOp<String> array = 3,584 B.
      core::String payloads[TextEditorProps::kMaxLines];
      std::size_t start = 0;
      for (unsigned short i = 0; i <= breaks; ++i)
      {
        std::size_t stop = start;
        while (stop < size && scratch.data()[stop] != '\r')
          ++stop;
        payloads[i] = core::String::Utf8(scratch.data() + start, stop - start);
        start = stop + 1;
      }
      const RowCursor caret = after ? *after
                                    : RowCursor(first + breaks,
                                                breaks ? static_cast<LineCursor::Column>(lastColumn)
                                                       : from.column + static_cast<LineCursor::Column>(normalized));
      if (!caret.isNone())
      {
        if (caret.row >= finalCount)
          return EDITOR_INVALID_CURSOR;
        const bool inReplacement = caret.row >= first && caret.row <= first + breaks;
        const unsigned short oldRow = caret.row < first ? caret.row : caret.row + (last - first) - breaks;
        const LineBytes row(inReplacement ? payloads[caret.row - first] : lines.at(oldRow).value);
        if (row.result() != EDITOR_OK)
          return row.result();
        if (caret.column < 0 || static_cast<std::size_t>(caret.column) > row.size())
          return EDITOR_INVALID_CURSOR;
      }
      ReplaceOpCursor ops(lines, first, last, payloads, breaks);
      return this->commit(ops, caret);
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
