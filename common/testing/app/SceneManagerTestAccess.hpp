#ifndef LOKA_APP_TESTING_SCENE_MANAGER_TEST_ACCESS_HPP
#define LOKA_APP_TESTING_SCENE_MANAGER_TEST_ACCESS_HPP

#include "app/core/SceneManager.hpp"

namespace loka
{
  namespace app
  {
    namespace testing
    {
      /** Testing-only reader for SceneManager's transaction queue. */
      class SceneManagerTestAccess
      {
      public:
        static size_t pendingTransactionCount(const SceneManager &manager)
        {
          return manager.pendingTransactions_.getRef().size();
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka

#endif // LOKA_APP_TESTING_SCENE_MANAGER_TEST_ACCESS_HPP
