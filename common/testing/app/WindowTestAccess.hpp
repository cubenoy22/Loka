#ifndef LOKA_APP_TESTING_WINDOW_TEST_ACCESS_HPP
#define LOKA_APP_TESTING_WINDOW_TEST_ACCESS_HPP

#ifndef TEST_BUILD
#error WindowTestAccess is available only in TEST_BUILD
#endif

#include "app/core/Window.hpp"

namespace loka
{
  namespace app
  {
    namespace testing
    {
      /** Test-only access to publish a native window-frame fact. */
      class WindowTestAccess
      {
      public:
        static void storeNativeFrame(::Window &window, const loka::core::Frame &frame)
        {
          window.storeNativeFrame(frame);
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka

#endif // LOKA_APP_TESTING_WINDOW_TEST_ACCESS_HPP
