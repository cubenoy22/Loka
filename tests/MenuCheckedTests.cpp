#include "MenuCheckedTests.hpp"

#include "app/Menu.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/MenuController.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "support/TestVerify.hpp"

#include <cassert>

namespace
{
  class CheckedMenuBoundary : public loka::app::MenuBoundary
  {
  public:
    CheckedMenuBoundary()
        : displayMode_(0)
    {
    }

    virtual void composeMenu(loka::app::MenuComposition &composition)
    {
      using namespace loka::app;
      if (!this->displayMode_)
      {
        this->displayMode_ = &this->dangerouslyUseState<int>(0);
      }
      composition << (Menu("View")
                      << MenuItem("Fit to Window").attr(MenuItemAttr().checked(this->displayMode_->get() == 0))
                      << MenuItem("Actual Size").attr(MenuItemAttr().checked(this->displayMode_->get() == 1))
                      << MenuItem("Actual Size (Scroll)").attr(MenuItemAttr().checked(this->displayMode_->get() == 2)));
    }

    void setDisplayMode(int value)
    {
      assert(this->displayMode_);
      loka::core::StateTrackerGuard guard(this->tracker());
      this->displayMode_->set(value);
    }

    loka::core::PushStateTracker *pushTracker()
    {
      return static_cast<loka::core::PushStateTracker *>(this->tracker());
    }

  private:
    loka::core::MutableState<int> *displayMode_;
  };

  class CheckedMenuConfig : public AppConfigurable
  {
  public:
    CheckedMenuConfig()
        : AppConfigurable(0),
          menu()
    {
    }

    virtual void compose(AppComposition &)
    {
    }

    virtual void composeMenu(loka::app::MenuComposition &composition)
    {
      composition << this->menu;
    }

    CheckedMenuBoundary menu;
  };

  void CountCheckedMenuApply(void *userData, Window *)
  {
    ++*static_cast<int *>(userData);
  }

  const loka::app::MenuDefinition *singleViewMenu(const loka::app::MenuBarDefinition *bar)
  {
    if (!bar || bar->menusCount() != 1)
    {
      return 0;
    }
    return bar->menuAt(0);
  }

} // namespace

void testMenuItemCheckedAttrProjectsValueAndState()
{
  loka::app::MenuItemDefinition defaultItem = loka::app::MenuItem("Default");
  LOKA_VERIFY(!defaultItem.isCheckedInitial());
  LOKA_VERIFY(defaultItem.checkedBindingState() == 0);

  loka::app::MenuItemDefinitionWithAttr checkedValue =
      loka::app::MenuItem("Value").attr(loka::app::MenuItemAttr().checked(true));
  LOKA_VERIFY(checkedValue.isCheckedInitial());
  LOKA_VERIFY(checkedValue.checkedBindingState() == 0);

  loka::core::MutableState<bool> checkedState(false);
  loka::core::PushStateTracker checkedTracker;
  checkedTracker.addState(&checkedState);
  loka::app::MenuItemDefinitionWithAttr checkedByState =
      loka::app::MenuItem("State").attr(loka::app::MenuItemAttr().checked(&checkedState));
  LOKA_VERIFY(!checkedByState.isCheckedInitial());
  LOKA_VERIFY(checkedByState.checkedBindingState() == &checkedState);
  {
    loka::core::StateTrackerGuard guard(&checkedTracker);
    checkedState.set(true);
  }
  LOKA_VERIFY(checkedByState.isCheckedInitial());
}

void testMenuBoundaryCheckedValuesSwapOnTrackedStateRefresh()
{
  CheckedMenuConfig config;
  int applyCount = 0;
  MenuController controller(&config, &CountCheckedMenuApply, &applyCount);
  controller.requestInvalidation();
  LOKA_VERIFY(controller.flushInvalidation(0));

  const loka::app::MenuDefinition *view = singleViewMenu(controller.defaultMenuBar());
  LOKA_VERIFY(view != 0);
  LOKA_VERIFY(view->itemsCount() == 3);
  LOKA_VERIFY(view->itemsHead()->isCheckedInitial());
  LOKA_VERIFY(!view->itemsHead()->nextInComposition->isCheckedInitial());
  LOKA_VERIFY(!view->itemsHead()->nextInComposition->nextInComposition->isCheckedInitial());
  const int initialApplyCount = applyCount;

  config.menu.setDisplayMode(2);
  LOKA_VERIFY(controller.flushInvalidation(0));
  view = singleViewMenu(controller.defaultMenuBar());
  LOKA_VERIFY(view != 0);
  LOKA_VERIFY(!view->itemsHead()->isCheckedInitial());
  LOKA_VERIFY(!view->itemsHead()->nextInComposition->isCheckedInitial());
  LOKA_VERIFY(view->itemsHead()->nextInComposition->nextInComposition->isCheckedInitial());
  LOKA_VERIFY(controller.diff().valid);
  LOKA_VERIFY(!controller.diff().fullRebuild);
  LOKA_VERIFY(controller.diff().changedCount() == 1);
  LOKA_VERIFY(controller.diff().changedHead()->value == 0);
  LOKA_VERIFY(applyCount == initialApplyCount + 1);
}

void testMenuControllerDisarmsTrackedMenuBoundaryBeforeDestruction()
{
  CheckedMenuConfig config;
  int applyCount = 0;
  MenuController *controller = new MenuController(&config, &CountCheckedMenuApply, &applyCount);
  LOKA_VERIFY(controller != 0);
  controller->requestInvalidation();
  LOKA_VERIFY(controller->flushInvalidation(0));
  const int applyCountBeforeDestruction = applyCount;

  delete controller;

  LOKA_VERIFY(config.menu.pushTracker()->invalidatesTarget(0));
  config.menu.setDisplayMode(2);
  LOKA_VERIFY(applyCount == applyCountBeforeDestruction);
}
