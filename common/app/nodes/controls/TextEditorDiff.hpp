#ifndef LOKA_APP_TEXT_EDITOR_DIFF_HPP
#define LOKA_APP_TEXT_EDITOR_DIFF_HPP
#include <string>

namespace loka
{
  namespace app
  {
    /** Completed differing range in two CR-separated logical documents.
        Empty text is one empty line. No native window participates. */
    class TextEditorLineDiff
    {
    public:
      TextEditorLineDiff(int first, int before, int after)
          : first_(first),
            before_(before),
            after_(after)
      {
      }
      int first() const
      {
        return this->first_;
      }
      int before() const
      {
        return this->before_;
      }
      int after() const
      {
        return this->after_;
      }

    private:
      int first_, before_, after_;
    };

    /** Slice a logical line from the same snapshot used for range detection. */
    inline std::string TextEditorLogicalLine(const std::string &text, int index)
    {
      std::size_t start = 0;
      for (int i = 0; i < index; ++i)
      {
        const std::size_t end = text.find('\r', start);
        if (end == std::string::npos)
          return std::string();
        start = end + 1;
      }
      const std::size_t end = text.find('\r', start);
      return text.substr(start, end == std::string::npos ? end : end - start);
    }

    /** The optional pre-edit caret (logical line and byte column) breaks ties
        only for an edge split/join that reproduces the complete after snapshot.
        An absent or unrelated hint leaves text-only detection unchanged. */
    inline TextEditorLineDiff
    DiffTextEditorLines(const std::string &before, const std::string &after, int caretLine = -1, int caretColumn = -1)
    {
      int oldCount = 1, newCount = 1;
      for (std::size_t i = 0; i < before.size(); ++i)
        if (before[i] == '\r')
          ++oldCount;
      for (std::size_t i = 0; i < after.size(); ++i)
        if (after[i] == '\r')
          ++newCount;
      int prefix = 0;
      std::size_t oldStart = 0, newStart = 0;
      while (prefix < oldCount && prefix < newCount)
      {
        std::size_t oldEnd = before.find('\r', oldStart), newEnd = after.find('\r', newStart);
        if (oldEnd == std::string::npos)
          oldEnd = before.size();
        if (newEnd == std::string::npos)
          newEnd = after.size();
        if (before.compare(oldStart, oldEnd - oldStart, after, newStart, newEnd - newStart) != 0)
          break;
        ++prefix;
        oldStart = oldEnd + 1;
        newStart = newEnd + 1;
      }
      int suffix = 0;
      std::size_t oldEnd = before.size(), newEnd = after.size();
      while (suffix < oldCount - prefix && suffix < newCount - prefix)
      {
        oldStart = oldEnd ? before.rfind('\r', oldEnd - 1) : std::string::npos;
        newStart = newEnd ? after.rfind('\r', newEnd - 1) : std::string::npos;
        oldStart = oldStart == std::string::npos ? 0 : oldStart + 1;
        newStart = newStart == std::string::npos ? 0 : newStart + 1;
        if (before.compare(oldStart, oldEnd - oldStart, after, newStart, newEnd - newStart) != 0)
          break;
        ++suffix;
        oldEnd = oldStart ? oldStart - 1 : 0;
        newEnd = newStart ? newStart - 1 : 0;
      }
      if (oldCount == prefix && newCount == prefix)
        return TextEditorLineDiff(prefix, 0, 0);
      // A split/join at an empty edge still needs the retained line identity.
      if (oldCount == prefix + suffix || newCount == prefix + suffix)
      {
        if (caretLine >= 0 && caretLine < oldCount && caretColumn >= 0)
        {
          std::size_t start = 0;
          for (int line = 0; line < caretLine; ++line)
            start = before.find('\r', start) + 1;
          const std::size_t end = before.find('\r', start);
          const std::size_t length = (end == std::string::npos ? before.size() : end) - start;
          if (static_cast<std::size_t>(caretColumn) <= length)
          {
            const std::size_t position = start + static_cast<std::size_t>(caretColumn);
            if (newCount == oldCount + 1 && after.size() == before.size() + 1 && after[position] == '\r'
                && before.compare(0, position, after, 0, position) == 0
                && before.compare(position, std::string::npos, after, position + 1, std::string::npos) == 0)
              return TextEditorLineDiff(caretLine, 1, 2);
            // Backspace joins the caret line into its predecessor; forward
            // delete at the other edge joins the following line into this one.
            const std::size_t seam = caretColumn == 0 && caretLine > 0 ? start - 1 : position;
            if (oldCount == newCount + 1 && before.size() == after.size() + 1 && seam < before.size()
                && before[seam] == '\r' && before.compare(0, seam, after, 0, seam) == 0
                && before.compare(seam + 1, std::string::npos, after, seam, std::string::npos) == 0)
              return TextEditorLineDiff(seam < start ? caretLine - 1 : caretLine, 2, 1);
          }
        }
        if (prefix)
          --prefix;
        else if (suffix)
          --suffix;
      }
      return TextEditorLineDiff(prefix, oldCount - prefix - suffix, newCount - prefix - suffix);
    }
  } // namespace app
} // namespace loka
#endif
