#include "ToolboxScenePlatformContractTests.hpp"

#include "support/TestVerify.hpp"

#include "../apple/toolbox/src/ToolboxEnabledChangeDispatch.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace
{
  class ToolboxEnabledChangeProbe
  {
  public:
    ToolboxEnabledChangeProbe()
        : bindingPath_(this)
    {
      for (int i = 0; i < TOOLBOX_ENABLED_CONTROL_KIND_COUNT; ++i)
      {
        for (int row = 0; row < 3; ++row)
        {
          this->bindings_[i][row].enabled = 0;
          this->bindings_[i][row].projectedEnabled = true;
        }
      }
    }

    void bind(loka::core::State<bool> *enabled)
    {
      this->bindingPath_.bind(enabled);
    }

    bool enabledChangeDispatchReady() const
    {
      return true;
    }

    bool applyEnabledChangeForKind(ToolboxEnabledControlKind kind,
                                   loka::core::State<bool> *enabled)
    {
      return ApplyToolboxEnabledChangeToBindings(
          this->bindings_[kind], this->bindings_[kind] + 3, enabled,
          *this, &ToolboxEnabledChangeProbe::apply);
    }

    struct Binding
    {
      loka::core::State<bool> *enabled;
      bool projectedEnabled;
    };

    void apply(Binding &binding)
    {
      binding.projectedEnabled = binding.enabled->get();
    }

    Binding bindings_[TOOLBOX_ENABLED_CONTROL_KIND_COUNT][3];

  private:
    ToolboxEnabledStateBindingPath<ToolboxEnabledChangeProbe,
                                   loka::core::State<bool> > bindingPath_;
  };
} // namespace

void testToolboxEnabledChangeUpdatesEveryMatchingControlKind()
{
  loka::core::PushStateTracker tracker;
  loka::core::MutableState<bool> enabled(true);
  loka::core::MutableState<bool> unrelated(false);
  ToolboxEnabledChangeProbe probe;
  for (int kind = 0; kind < TOOLBOX_ENABLED_CONTROL_KIND_COUNT; ++kind)
  {
    probe.bindings_[kind][0].enabled = &enabled;
    probe.bindings_[kind][1].enabled = &unrelated;
    probe.bindings_[kind][2].enabled = &enabled;
  }
  probe.bind(&enabled);
  {
    loka::core::StateTrackerGuard transaction(&tracker);
    enabled.set(false);
  }

  for (int kind = 0; kind < TOOLBOX_ENABLED_CONTROL_KIND_COUNT; ++kind)
  {
    LOKA_VERIFY(!probe.bindings_[kind][0].projectedEnabled);
    LOKA_VERIFY(!probe.bindings_[kind][2].projectedEnabled);
    LOKA_VERIFY(probe.bindings_[kind][1].projectedEnabled);
    LOKA_VERIFY(probe.applyEnabledChangeForKind(
        static_cast<ToolboxEnabledControlKind>(kind), &enabled));
    LOKA_VERIFY(!probe.applyEnabledChangeForKind(
        static_cast<ToolboxEnabledControlKind>(kind), 0));
    LOKA_VERIFY(!ApplyToolboxEnabledChangeToBindings(
        probe.bindings_[kind], probe.bindings_[kind],
        static_cast<loka::core::State<bool> *>(&enabled),
        probe, &ToolboxEnabledChangeProbe::apply));
  }
}
