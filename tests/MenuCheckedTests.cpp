#include "MenuCheckedTests.hpp"

#include "../example/SimpleViewer/src/MainNode.hpp"
#include "app/Menu.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/MenuController.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

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

  class HeapCheckedMenuConfig : public AppConfigurable
  {
  public:
    HeapCheckedMenuConfig()
        : AppConfigurable(0),
          menu_(new CheckedMenuBoundary())
    {
    }

    virtual ~HeapCheckedMenuConfig()
    {
      delete this->menu_;
    }

    virtual void compose(AppComposition &)
    {
    }

    virtual void composeMenu(loka::app::MenuComposition &composition)
    {
      if (this->menu_)
      {
        composition << *this->menu_;
      }
    }

    CheckedMenuBoundary *menu()
    {
      return this->menu_;
    }

    void destroyMenu()
    {
      delete this->menu_;
      this->menu_ = 0;
    }

  private:
    CheckedMenuBoundary *menu_;
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

  loka::app::ImageViewNode *findOnlyImageView(loka::app::scene::Node *node)
  {
    if (!node)
    {
      return 0;
    }
    if (node->asImageViewNode())
    {
      return node->asImageViewNode();
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
         child;
         child = child->nextInComposition)
    {
      loka::app::ImageViewNode *found = findOnlyImageView(child);
      if (found)
      {
        return found;
      }
    }
    return 0;
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

void testMenuBoundaryRefreshSurvivesMenuControllerReplacement()
{
  CheckedMenuConfig config;
  int applyCount = 0;
  {
    MenuController controller(&config, &CountCheckedMenuApply, &applyCount);
    controller.requestInvalidation();
    LOKA_VERIFY(controller.flushInvalidation(0));
    const loka::app::MenuDefinition *view = singleViewMenu(controller.defaultMenuBar());
    LOKA_VERIFY(view != 0);
    LOKA_VERIFY(view->itemsHead()->isCheckedInitial());
    LOKA_VERIFY(!view->itemsHead()->nextInComposition->isCheckedInitial());
    LOKA_VERIFY(!view->itemsHead()->nextInComposition->nextInComposition->isCheckedInitial());
  }

  config.menu.setDisplayMode(2);
  LOKA_VERIFY(config.menuRefresh().hasPendingRequest());

  MenuController replacement(&config, &CountCheckedMenuApply, &applyCount);
  LOKA_VERIFY(replacement.flushInvalidation(0));
  const loka::app::MenuDefinition *view = singleViewMenu(replacement.defaultMenuBar());
  LOKA_VERIFY(view != 0);
  LOKA_VERIFY(!view->itemsHead()->isCheckedInitial());
  LOKA_VERIFY(!view->itemsHead()->nextInComposition->isCheckedInitial());
  LOKA_VERIFY(view->itemsHead()->nextInComposition->nextInComposition->isCheckedInitial());
}

void testMenuControllerOutlivedByBoundaryDoesNotTouchIt()
{
  HeapCheckedMenuConfig config;
  int applyCount = 0;
  MenuController controller(&config, &CountCheckedMenuApply, &applyCount);
  controller.requestInvalidation();
  LOKA_VERIFY(controller.flushInvalidation(0));

  config.destroyMenu();
}

void testSimpleViewerDisplayModeUpdatesRetainedImageViewProps()
{
  NullScenePlatformController platform;
  NullPlatformContext platformContext;
  loka::core::EmitterState openDialogEvent;
  loka::core::MutableState<simpleviewer::DisplayMode> displayMode(
      simpleviewer::DISPLAY_FIT);
  loka::core::EmitterState fitEvent;
  loka::core::EmitterState actualEvent;
  loka::core::EmitterState actualScrollEvent;
  loka::core::PushStateTracker modeTracker;
  modeTracker.addState(&displayMode);
  simpleviewer::MainProps props;
  props.platformContext(&platformContext)
      .openDialogEvent(&openDialogEvent)
      .displayMode(&displayMode)
      .fitEvent(&fitEvent)
      .actualEvent(&actualEvent)
      .actualScrollEvent(&actualScrollEvent);
  loka::app::scene::NodeDefinitionBase *rootDefinition =
      loka::app::scene::Boundary<simpleviewer::MainNode>(props).clone();
  LOKA_VERIFY(rootDefinition != 0);
  loka::app::scene::Scene scene(rootDefinition);
  scene.mount(&platform);
  scene.updateAttached(true);

  loka::app::ImageViewNode *imageView = findOnlyImageView(
      loka::dsl::testing::SceneTestAccess::rootNode(scene));
  LOKA_VERIFY(imageView != 0);
  LOKA_VERIFY(imageView->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT);
  loka::app::ImageViewNode *retainedImageView = imageView;

  {
    loka::core::StateTrackerGuard guard(&modeTracker);
    displayMode.set(simpleviewer::DISPLAY_ACTUAL);
  }
  if (scene.hasPendingInvalidation())
  {
    LOKA_VERIFY(scene.flushInvalidation());
  }
  imageView = findOnlyImageView(loka::dsl::testing::SceneTestAccess::rootNode(scene));
  LOKA_VERIFY(imageView != 0);
  // Each display mode owns a distinct Match arm resident; the original Fit
  // image is parked while Actual is active, then reused on re-entry.
  LOKA_VERIFY(imageView != retainedImageView);
  LOKA_VERIFY(imageView->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_INTRINSIC);

  {
    loka::core::StateTrackerGuard guard(&modeTracker);
    displayMode.set(simpleviewer::DISPLAY_FIT);
  }
  if (scene.hasPendingInvalidation())
  {
    LOKA_VERIFY(scene.flushInvalidation());
  }
  imageView = findOnlyImageView(loka::dsl::testing::SceneTestAccess::rootNode(scene));
  LOKA_VERIFY(imageView != 0);
  LOKA_VERIFY(imageView == retainedImageView);
  LOKA_VERIFY(imageView->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT);

  scene.unmount();
}
