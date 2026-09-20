#ifndef LOKA_APP_TEXT_SHAPING_HPP
#define LOKA_APP_TEXT_SHAPING_HPP

namespace loka
{
  namespace app
  {
    /** Rail-owned projection policy; never a value or DSL property. */
    enum TextShaping
    {
      PER_RUN,
      WHOLE_LINE
    };

  } // namespace app
} // namespace loka
#endif
