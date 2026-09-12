#ifndef LOKA_MAC_WINDOW_TEST_ACCESS_HPP
#define LOKA_MAC_WINDOW_TEST_ACCESS_HPP

#include "../MacWindow.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      /** Test-only access to native identities owned by MacWindow. */
      class MacWindowTestAccess
      {
      public:
        static void *nativeWindow(const ::MacWindow &window)
        {
          return window.window_;
        }

        static void *contentView(const ::MacWindow &window)
        {
          return window.contentView_;
        }
      };
    } // namespace testing
  } // namespace dsl
} // namespace loka

#endif // LOKA_MAC_WINDOW_TEST_ACCESS_HPP
