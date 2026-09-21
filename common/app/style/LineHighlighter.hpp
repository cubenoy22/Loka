#ifndef LOKA_APP_LINE_HIGHLIGHTER_HPP
#define LOKA_APP_LINE_HIGHLIGHTER_HPP
#include "app/style/AttributedString.hpp"
namespace loka
{
  namespace app
  {
    /** Borrowed app-owned policy; must outlive every projection using it. */
    class LineHighlighter
    {
    public:
      virtual ~LineHighlighter() {}
      virtual bool highlight(const core::String &line, AttributedString::Builder &out) const = 0;
    };
    /** Completed disposable projection. Shared input identity skips the Builder.
        A failed style build yields plain text; it cannot undo a document edit. */
    class LineHighlight
    {
    public:
      LineHighlight(const core::String &input, const LineHighlighter *policy, const LineHighlight *previous = 0)
          : input_(input),
            policy_(policy),
            output_()
      {
        if (previous && policy == previous->policy_
            && input.compare(previous->input_, false) == core::StringCompareEqual)
        {
          this->output_ = previous->output_;
          return;
        }
        if (policy)
        {
          AttributedString::Builder builder(1);
          if (policy->highlight(input, builder))
          {
            this->output_ = builder.build();
            if (this->output_.valid())
              return;
          }
        }
        this->output_ = Styled(input, TextStyle());
      }
      const AttributedString &value() const
      {
        return this->output_;
      }

    private:
      core::String input_;
      const LineHighlighter *policy_;
      AttributedString output_;
    };
  } // namespace app
} // namespace loka
#endif
