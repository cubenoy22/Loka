#include "ToolboxScenePlatformContractTests.hpp"

#include "support/TestVerify.hpp"

#include "../apple/toolbox/src/ToolboxPopupSelectionInput.hpp"

namespace
{
  struct ToolboxPopupSelectionDouble
      : public loka::core::DerivedState<int>::EvalFn
  {
    explicit ToolboxPopupSelectionDouble(loka::core::State<int> *source)
        : source_(source)
    {
    }

    virtual int operator()()
    {
      return this->source_->get() * 2;
    }

    loka::core::State<int> *source_;
  };
} // namespace

void testToolboxPopupSelectionWriteUsesTrackerTransaction()
{
  loka::core::MutableState<int> selectedIndex(0);
  loka::core::DerivedState<int> doubled(
      &selectedIndex,
      new ToolboxPopupSelectionDouble(&selectedIndex));
  loka::core::PushStateTracker tracker;
  tracker.addState(&selectedIndex);
  tracker.addState(&doubled);

  PublishToolboxPopupSelection(&tracker, selectedIndex, 0, 3);

  LOKA_VERIFY(selectedIndex.get() == 3);
  LOKA_VERIFY(doubled.get() == 6);
}
