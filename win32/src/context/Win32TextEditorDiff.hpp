#ifndef LOKA_WIN32_TEXT_EDITOR_DIFF_HPP
#define LOKA_WIN32_TEXT_EDITOR_DIFF_HPP
#include <string>

namespace loka
{
  namespace win32
  {
    /** Completed differing range in two CR-separated logical documents.
        Empty text is one empty line. No caret or native window participates. */
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

    inline TextEditorLineDiff DiffTextEditorLines(const std::string &before, const std::string &after)
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
        if (prefix)
          --prefix;
        else if (suffix)
          --suffix;
      }
      return TextEditorLineDiff(prefix, oldCount - prefix - suffix, newCount - prefix - suffix);
    }
  } // namespace win32
} // namespace loka
#endif
