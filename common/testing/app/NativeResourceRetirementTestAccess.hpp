#ifndef LOKA_TESTING_APP_NATIVE_RESOURCE_RETIREMENT_TEST_ACCESS_HPP
#define LOKA_TESTING_APP_NATIVE_RESOURCE_RETIREMENT_TEST_ACCESS_HPP

#include "app/PlatformContext.hpp"
#include <cstddef>

namespace loka
{
  namespace app
  {
    namespace testing
    {
      /** Ledger inspection and recursive-drain probe, outside the production API. */
      class NativeResourceRetirementTestAccess
      {
      public:
        static std::size_t held(const PlatformContext &context)
        {
          return count(context.nativeResourceRetirement_.held_);
        }
        static std::size_t queued(const PlatformContext &context)
        {
          return count(context.nativeResourceRetirement_.queued_);
        }
        static std::size_t inFlight(const PlatformContext &context)
        {
          return count(context.nativeResourceRetirement_.inFlight_);
        }
        static void drain(PlatformContext &context)
        {
          context.nativeResourceRetirement_.drain();
        }

      private:
        static std::size_t count(const NativeResourceRetirement::Ticket *ticket)
        {
          std::size_t result = 0;
          for (; ticket; ticket = ticket->next)
            ++result;
          return result;
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka
#endif
