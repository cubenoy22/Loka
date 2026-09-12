#ifndef LOKA_TESTING_APP_TEST_ACCESS_HPP
#define LOKA_TESTING_APP_TEST_ACCESS_HPP

#include "app/core/App.hpp"

namespace loka
{
  namespace app
  {
    namespace testing
    {
      /** Scenario pumps use the production App admission between driver steps. */
      class AppTestAccess
      {
      public:
        static size_t pendingWindowCloseCount(const App &app)
        {
          return app.pendingWindowClosures_.size();
        }

        static void flushWindowInvalidations(App &app)
        {
          app.flushWindowInvalidations();
        }
      };
    }
  }
}

#endif
