#include "ToolboxScenePlatformContractTests.hpp"

#include "support/TestVerify.hpp"

#include "../apple/toolbox/src/ToolboxPopupSelectionInput.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/composition/NodeComposition.hpp"

namespace
{
  class ToolboxPopupStateOwnerBoundary
      : public loka::app::scene::BoundaryNode
  {
  public:
    virtual void composeWithContext(
        loka::app::scene::ComponentContext &,
        loka::app::scene::ComposeEvent)
    {
    }
  };

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
  ToolboxPopupStateOwnerBoundary owner;
  loka::app::scene::NodeState<int> selectedIndex;
  {
    loka::app::scene::NodeComposition::StateBatch states(&owner);
    states.state(selectedIndex, 0);
  }
  loka::core::State<int> *selectedState = selectedIndex.state();
  LOKA_VERIFY(selectedState != 0);
  loka::core::MutableState<int> *mutableSelected =
      static_cast<loka::core::MutableState<int> *>(
          selectedState->asMutableState());
  LOKA_VERIFY(mutableSelected != 0);
  loka::core::DerivedState<int> doubled(
      selectedState,
      new ToolboxPopupSelectionDouble(selectedState));
  owner.tracker()->asPushTracker()->addState(&doubled);

  // The window tracker deliberately owns only a window fact. The popup State
  // is the ordinary NodeState above, registered by its Boundary StateBatch.
  loka::core::MutableState<bool> windowVisible(true);
  loka::core::PushStateTracker windowTracker;
  windowTracker.addState(&windowVisible);

  PublishToolboxPopupSelection(
      &windowTracker, *mutableSelected, 0, 3);

  LOKA_VERIFY(selectedIndex.get() == 3);
  LOKA_VERIFY(doubled.get() == 6);
}
