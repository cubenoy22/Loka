#ifndef LOKA_APP_TESTING_SCENE_MANAGER_TEST_ACCESS_HPP
#define LOKA_APP_TESTING_SCENE_MANAGER_TEST_ACCESS_HPP

#include "app/core/SceneManager.hpp"
#include "app/scene/state/NodeState.hpp"

namespace loka
{
  namespace app
  {
    namespace testing
    {
      /** Testing-only readers for SceneManager transaction state. */
      class SceneManagerTestAccess
      {
      public:
        static bool hasPendingRequest(const SceneManager &manager)
        {
          return manager.request_ != SceneManager::REQUEST_NONE;
        }

        static const ::loka::core::PushStateTracker &tracker(const SceneManager &manager)
        {
          return manager.tracker_;
        }

        /** Exercises a seat write on the manager's actual registered Scene fact. */
        static void writeCurrentScene(SceneManager &manager)
        {
          loka::app::scene::NodeState<loka::app::scene::Scene *> current(
              &manager.currentScene_, &manager.tracker_);
          current.set(manager.currentScene_.get(), true);
        }

        /** Number of scenes waiting in the Window retirement pool. */
        static size_t retiredSceneCount(const SceneManager &manager)
        {
          return manager.retiredScenes_.size();
        }

        static loka::app::scene::Scene *desiredScene(const SceneManager &manager)
        {
          return manager.desired_;
        }

#ifdef TEST_BUILD
        /** Last refused identity, valid until the next App admission or manager destruction. */
        static loka::app::scene::Scene *lastPrepareRefusal(const SceneManager &manager)
        {
          return manager.lastPrepareRefusal_;
        }
#endif

        static ::loka::core::TrackerPhase trackerPhase(const SceneManager &manager)
        {
          return manager.tracker_.phase();
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka

#endif // LOKA_APP_TESTING_SCENE_MANAGER_TEST_ACCESS_HPP
