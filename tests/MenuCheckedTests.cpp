#include "MenuCheckedTests.hpp"

#include "../example/SimpleViewer/src/MainNode.hpp"
#include "app/Menu.hpp"
#include "app/core/AppConfigurable.hpp"
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

void testSimpleViewerDisplayModeUpdatesRetainedImageViewProps()
{
  NullScenePlatformController platform;
  NullPlatformContext platformContext;
  loka::core::EmitterState openDialogEvent;
  simpleviewer::MainProps props;
  props.platformContext(&platformContext)
      .openDialogEvent(&openDialogEvent);
  loka::app::scene::NodeDefinitionBase *rootDefinition =
      loka::app::scene::Boundary<simpleviewer::MainNode>(props).clone();
  LOKA_VERIFY(rootDefinition != 0);
  loka::app::scene::Scene scene(rootDefinition);
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);

  loka::app::ImageViewNode *imageView = findOnlyImageView(
      loka::dsl::testing::SceneTestAccess::rootNode(scene));
  LOKA_VERIFY(imageView != 0);
  LOKA_VERIFY(imageView->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT);
  loka::app::ImageViewNode *retainedImageView = imageView;

  LOKA_VERIFY(scene.menuBar());
  scene.menuBar()->menuAt(2)->itemsHead()->nextInComposition->onClickState->emit();
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

  scene.menuBar()->menuAt(2)->itemsHead()->onClickState->emit();
  if (scene.hasPendingInvalidation())
  {
    LOKA_VERIFY(scene.flushInvalidation());
  }
  imageView = findOnlyImageView(loka::dsl::testing::SceneTestAccess::rootNode(scene));
  LOKA_VERIFY(imageView != 0);
  LOKA_VERIFY(imageView == retainedImageView);
  LOKA_VERIFY(imageView->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT);

  loka::dsl::testing::SceneTestAccess::unmount(scene);
}
