#ifndef LOKA_TESTS_PLATFORM_NULL_BUTTON_CONTEXT_HPP
#define LOKA_TESTS_PLATFORM_NULL_BUTTON_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"

namespace loka
{
  namespace app
  {
    class ButtonNode;
  }
  namespace app
  {
    namespace scene
    {
      class IPlatformController;
    }
  }
}

class NullButtonContext : public loka::app::scene::NativeNodeContext
{
public:
  NullButtonContext(loka::app::ButtonNode *node, NullScenePlatformController *controller);
  virtual ~NullButtonContext();

  /** Attach-time read (late-subscriber rule): presentation from the
      current fact, called by the installing handler after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  virtual short layout(loka::app::scene::IPlatformController *controller,
                       loka::app::scene::LayoutState &state);

  void observeNodeLifetimeHint();

private:
  loka::app::ButtonNode *node_;
  NullScenePlatformController *controller_;
  NullScenePlatformController::FakeControlHandle *handle_;
};

void RegisterNullButtonNodeHandler(NullScenePlatformController &controller);

#endif // LOKA_TESTS_PLATFORM_NULL_BUTTON_CONTEXT_HPP
