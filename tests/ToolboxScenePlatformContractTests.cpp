#include "ToolboxScenePlatformContractTests.hpp"

#include "support/TestVerify.hpp"

#include "../apple/toolbox/src/ToolboxEnabledChangeDispatch.hpp"
#include "core/State.hpp"

namespace
{
  class ToolboxEnabledChangeProbe
  {
  public:
    ToolboxEnabledChangeProbe()
    {
      for (int i = 0; i < TOOLBOX_ENABLED_CONTROL_KIND_COUNT; ++i)
      {
        this->bindings_[i] = 0;
        this->updated_[i] = false;
      }
    }

    bool apply(ToolboxEnabledControlKind kind,
               loka::core::State<bool> *enabled)
    {
      if (this->bindings_[kind] != enabled)
      {
        return false;
      }
      this->updated_[kind] = true;
      return true;
    }

    loka::core::State<bool> *bindings_[TOOLBOX_ENABLED_CONTROL_KIND_COUNT];
    bool updated_[TOOLBOX_ENABLED_CONTROL_KIND_COUNT];
  };
} // namespace

void testToolboxEnabledChangeUpdatesEveryMatchingControlKind()
{
  loka::core::MutableState<bool> enabled(true);
  loka::core::State<bool> *enabledState = &enabled;
  ToolboxEnabledChangeProbe probe;
  probe.bindings_[TOOLBOX_ENABLED_POPUP_HIT] = enabledState;
  probe.bindings_[TOOLBOX_ENABLED_BUTTON_CONTROL] = enabledState;

  DispatchToolboxEnabledChange(
      probe,
      enabledState,
      &ToolboxEnabledChangeProbe::apply);

  LOKA_VERIFY(probe.updated_[TOOLBOX_ENABLED_POPUP_HIT]);
  LOKA_VERIFY(probe.updated_[TOOLBOX_ENABLED_BUTTON_CONTROL]);
}
