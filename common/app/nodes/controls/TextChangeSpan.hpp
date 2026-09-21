#ifndef LOKA_TEXT_CHANGE_SPAN_HPP
#define LOKA_TEXT_CHANGE_SPAN_HPP

#include <cstddef>

namespace loka
{
  namespace app
  {
    namespace detail
    {
      /** Minimal changed column span in two indexed character sequences.
          The unchanged suffix cannot overlap the common prefix. The after
          end is the replacement caret, including pure deletion. */
      class TextChangeSpan
      {
      public:
        template <class Before, class After>
        TextChangeSpan(const Before &before, std::size_t beforeLength, const After &after, std::size_t afterLength)
            : start_(0),
              beforeEnd_(beforeLength),
              afterEnd_(afterLength)
        {
          while (this->start_ < this->beforeEnd_ && this->start_ < this->afterEnd_
                 && before[this->start_] == after[this->start_])
            ++this->start_;
          while (this->beforeEnd_ > this->start_ && this->afterEnd_ > this->start_
                 && before[this->beforeEnd_ - 1] == after[this->afterEnd_ - 1])
          {
            --this->beforeEnd_;
            --this->afterEnd_;
          }
        }
        std::size_t start() const
        {
          return this->start_;
        }
        std::size_t beforeEnd() const
        {
          return this->beforeEnd_;
        }
        std::size_t afterEnd() const
        {
          return this->afterEnd_;
        }

      private:
        std::size_t start_;
        std::size_t beforeEnd_;
        std::size_t afterEnd_;
      };
    } // namespace detail
  } // namespace app
} // namespace loka
#endif
