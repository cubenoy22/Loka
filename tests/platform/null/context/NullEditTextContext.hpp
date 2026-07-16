#ifndef LOKA_TESTS_PLATFORM_NULL_EDIT_TEXT_CONTEXT_HPP
#define LOKA_TESTS_PLATFORM_NULL_EDIT_TEXT_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"

namespace loka
{
  namespace app
  {
    class EditTextNode;
  }
}

class NullEditTextContext : public loka::app::scene::NativeNodeContext
{
public:
  NullEditTextContext(loka::app::EditTextNode *node, NullScenePlatformController *controller);
  virtual ~NullEditTextContext();

  virtual void onNodeAttached();
  virtual void onNodeDetached();
  virtual short layout(loka::app::scene::IPlatformController *controller,
                       loka::app::scene::LayoutState &state);

  void observeNodeLifetimeHint();

private:
  loka::app::EditTextNode *node_;
  NullScenePlatformController *controller_;
  NullScenePlatformController::FakeControlHandle *handle_;
};

void RegisterNullEditTextNodeHandler(NullScenePlatformController &controller);

#endif // LOKA_TESTS_PLATFORM_NULL_EDIT_TEXT_CONTEXT_HPP
