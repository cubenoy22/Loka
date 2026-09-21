#ifndef LOKA_APP_LINE_CURSOR_HPP
#define LOKA_APP_LINE_CURSOR_HPP
#include "core/ObservableList.hpp"
namespace loka
{
  namespace app
  {
    /** Attachment-scoped line identity and column; none has no caret. */
    struct LineCursor
    {
      /** Stage 1 ASCII code-unit index; scoped to avoid the Column layout DSL. */
      typedef int Column;
      core::ItemId line;
      Column column;
      LineCursor(core::ItemId id = core::ItemId::none(), Column col = 0)
          : line(id),
            column(col)
      {
      }
      static LineCursor None()
      {
        return LineCursor();
      }
      bool isNone() const
      {
        return this->line.isNone();
      }
      bool operator==(const LineCursor &other) const
      {
        return this->line == other.line && this->column == other.column;
      }
      bool operator!=(const LineCursor &other) const
      {
        return !(*this == other);
      }
    };
  } // namespace app
} // namespace loka
#endif
