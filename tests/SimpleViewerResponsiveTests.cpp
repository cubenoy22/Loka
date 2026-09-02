#include "SimpleViewerResponsiveTests.hpp"

#include "../example/SimpleViewer/src/MyAppConfig.hpp"
#include "app/core/MenuController.hpp"
#include "app/layout/RowLayout.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/NullWindow.hpp"
#include "support/TestVerify.hpp"
#include "testing/app/WindowTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"

class SimpleViewerTestAccess
{
public:
  static void setNavMode(simpleviewer::MainNode &node, simpleviewer::NavMode mode)
  {
    loka::core::StateTrackerGuard guard(node.tracker());
    node.navMode_.set(mode);
  }

  static loka::core::State<int> *displayModeState(SimpleViewerAppConfig &config)
  {
    return config.menu_.displayMode_;
  }

  static loka::core::EmitterState *openDialogEvent(SimpleViewerAppConfig &config)
  {
    return &config.openDialogEvent_;
  }

  static loka::core::EmitterState *fitEvent(SimpleViewerAppConfig &config)
  {
    return &config.menu_.fitToWindowEvent_;
  }

  static loka::core::EmitterState *actualEvent(SimpleViewerAppConfig &config)
  {
    return &config.menu_.actualEvent_;
  }

  static loka::core::EmitterState *actualScrollEvent(SimpleViewerAppConfig &config)
  {
    return &config.menu_.actualScrollEvent_;
  }

  static void setDisplayMode(SimpleViewerAppConfig &config,
                             simpleviewer::DisplayMode mode)
  {
    loka::core::StateTrackerGuard guard(config.menu_.tracker());
    config.menu_.displayMode_->set(static_cast<int>(mode));
  }

  static void setLoadedImage(simpleviewer::MainNode &node,
                             const loka::core::resource::Image &image)
  {
    loka::core::StateTrackerGuard guard(node.tracker());
    node.commitLoadedImage(image);
  }
};

namespace
{
  const int kRowGap = 8;

  class NullImageGeometryContext : public loka::app::scene::NodeContext
  {
  public:
    explicit NullImageGeometryContext(loka::app::ImageViewNode *node)
        : node_(node),
          geometry_()
    {
    }

    virtual short layout(loka::app::scene::IPlatformController *,
                         loka::app::scene::LayoutState &state)
    {
      int width = state.width;
      int height = state.height;
      if (this->node_ && this->node_->props.hasAttr_ &&
          this->node_->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_INTRINSIC &&
          this->node_->props.image_)
      {
        const loka::core::resource::Image image = this->node_->props.image_->get();
        if (image.width() > 0)
        {
          width = image.width();
        }
        if (image.height() > 0)
        {
          height = image.height();
        }
      }
      this->geometry_ = state;
      this->geometry_.width = static_cast<short>(width);
      this->geometry_.height = static_cast<short>(height);
      state.width = static_cast<short>(width);
      state.height = static_cast<short>(height);
      return static_cast<short>(state.y + height);
    }

    const loka::app::scene::LayoutState &geometry() const
    {
      return this->geometry_;
    }

  private:
    loka::app::ImageViewNode *node_;
    loka::app::scene::LayoutState geometry_;
  };

  class NullImageGeometryHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(
        loka::app::scene::Node *node,
        loka::app::scene::IPlatformController *controller,
        const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::ImageViewNode *image = node ? node->asImageViewNode() : 0;
      if (!image || !controller)
      {
        return 0;
      }
      if (!image->getContext())
      {
        image->setContext(new NullImageGeometryContext(image));
      }
      return image->getContext();
    }
  };

  void CountMenuApply(void *userData, Window *)
  {
    ++*static_cast<int *>(userData);
  }

  loka::app::scene::Node *findNode(loka::app::scene::Node *node,
                                    const char *testId)
  {
    if (!node)
    {
      return 0;
    }
    if (node->testId() == testId)
    {
      return node;
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
         child;
         child = child->nextInComposition)
    {
      loka::app::scene::Node *found = findNode(child, testId);
      if (found)
      {
        return found;
      }
    }
    return 0;
  }

  void flushScene(loka::app::scene::Scene &scene)
  {
    if (scene.hasPendingInvalidation())
    {
      LOKA_VERIFY(scene.flushInvalidation());
    }
  }

  const loka::app::MenuDefinition *viewMenu(const loka::app::MenuBarDefinition *bar)
  {
    if (!bar || bar->menusCount() != 3)
    {
      return 0;
    }
    return bar->menuAt(2);
  }

  void verifyCheckedMode(const loka::app::MenuDefinition *view,
                         simpleviewer::DisplayMode mode)
  {
    LOKA_VERIFY(view != 0);
    LOKA_VERIFY(view->itemsCount() == 3);
    const loka::app::MenuItemDefinition *item = view->itemsHead();
    LOKA_VERIFY(item != 0);
    LOKA_VERIFY(item->isCheckedInitial() == (mode == simpleviewer::DISPLAY_FIT));
    item = item->nextInComposition;
    LOKA_VERIFY(item != 0);
    LOKA_VERIFY(item->isCheckedInitial() == (mode == simpleviewer::DISPLAY_ACTUAL));
    item = item->nextInComposition;
    LOKA_VERIFY(item != 0);
    LOKA_VERIFY(item->isCheckedInitial() == (mode == simpleviewer::DISPLAY_ACTUAL_SCROLL));
  }

  struct RowSeatRecorder
  {
    RowSeatRecorder()
        : navCalls(0),
          contentCalls(0),
          navState(),
          contentState(),
          navNode(0),
          contentNode(0)
    {
    }

    int navCalls;
    int contentCalls;
    loka::app::scene::LayoutState navState;
    loka::app::scene::LayoutState contentState;
    loka::app::scene::Node *navNode;
    loka::app::scene::Node *contentNode;
  };

  int recordRowSeat(void *context,
                    loka::app::scene::Node *node,
                    const loka::app::scene::LayoutState &state)
  {
    RowSeatRecorder *record = static_cast<RowSeatRecorder *>(context);
    if (node == record->navNode)
    {
      ++record->navCalls;
      record->navState = state;
    }
    if (node == record->contentNode)
    {
      ++record->contentCalls;
      record->contentState = state;
    }
    return state.y + state.height;
  }

  void recordRootSeats(loka::app::StackNode *rootRow,
                       RowSeatRecorder &record)
  {
    LOKA_VERIFY(rootRow != 0);
    record.navNode = rootRow->childrenHead();
    LOKA_VERIFY(record.navNode != 0);
    record.contentNode = record.navNode->nextInComposition;
    LOKA_VERIFY(record.contentNode != 0);
    loka::app::scene::Node *dialogSeat = record.contentNode->nextInComposition;
    LOKA_VERIFY(dialogSeat != 0);
    LOKA_VERIFY(dialogSeat->nextInComposition == 0);

    loka::app::scene::LayoutState state;
    state.width = 600;
    state.height = 400;
    state.spacing = kRowGap;
    loka::app::layout::RowLayoutMetrics metrics;
    metrics.gap = kRowGap;
    loka::app::layout::computeRowLayoutResultY(
        rootRow, state, metrics, &record, &recordRowSeat);
  }

  struct SimpleViewerHarness
  {
    SimpleViewerHarness()
        : platformContext(),
          config(&platformContext),
          imageGeometryHandler(),
          platform(),
          scene(0),
          menuApplyCount(0),
          menuController(&config, &CountMenuApply, &menuApplyCount)
    {
      LOKA_VERIFY(this->platform.registerNodeHandler(&this->imageGeometryHandler));
      simpleviewer::MainProps props;
      props.platformContext(&platformContext)
          .openDialogEvent(SimpleViewerTestAccess::openDialogEvent(config))
          .displayMode(SimpleViewerTestAccess::displayModeState(config))
          .fitEvent(SimpleViewerTestAccess::fitEvent(config))
          .actualEvent(SimpleViewerTestAccess::actualEvent(config))
          .actualScrollEvent(SimpleViewerTestAccess::actualScrollEvent(config));
      loka::app::scene::NodeDefinitionBase *rootDefinition =
          loka::app::scene::Boundary<simpleviewer::MainNode>(props).clone();
      LOKA_VERIFY(rootDefinition != 0);
      this->scene = new loka::app::scene::Scene(rootDefinition);
      LOKA_VERIFY(this->scene != 0);
      this->scene->mount(&this->platform);
      this->scene->updateAttached(true);
      this->menuController.requestInvalidation();
      LOKA_VERIFY(this->menuController.flushInvalidation(0));
    }

    ~SimpleViewerHarness()
    {
      if (this->scene)
      {
        this->scene->unmount();
        delete this->scene;
      }
    }

    simpleviewer::MainNode *mainNode() const
    {
      return static_cast<simpleviewer::MainNode *>(
          loka::dsl::testing::SceneTestAccess::rootNode(*this->scene));
    }

    NullPlatformContext platformContext;
    SimpleViewerAppConfig config;
    NullImageGeometryHandler imageGeometryHandler;
    NullScenePlatformController platform;
    loka::app::scene::Scene *scene;
    int menuApplyCount;
    MenuController menuController;
  };
} // namespace

void testSimpleViewerNavSeatsFollowModeAndRetainContentImage()
{
  SimpleViewerHarness harness;
  simpleviewer::MainNode *main = harness.mainNode();
  LOKA_VERIFY(main != 0);
  loka::app::StackNode *rootRow = static_cast<loka::app::StackNode *>(
      findNode(main, "SimpleViewer.RootRow"));
  LOKA_VERIFY(rootRow != 0);
  loka::app::ImageViewNode *retainedImage = static_cast<loka::app::ImageViewNode *>(
      findNode(main, "SimpleViewer.Image"));
  LOKA_VERIFY(retainedImage != 0);

  RowSeatRecorder wide;
  recordRootSeats(rootRow, wide);
  LOKA_VERIFY(wide.navCalls == 1);
  LOKA_VERIFY(wide.contentCalls == 1);
  LOKA_VERIFY(wide.navState.width == 200);
  LOKA_VERIFY(wide.contentState.x == 200 + kRowGap);

  SimpleViewerTestAccess::setNavMode(*main, simpleviewer::NAV_NARROW_CLOSED);
  flushScene(*harness.scene);
  LOKA_VERIFY(findNode(main, "SimpleViewer.NavPane") == 0);
  LOKA_VERIFY(findNode(main, "SimpleViewer.NavToggle") != 0);
  LOKA_VERIFY(findNode(main, "SimpleViewer.Image") == retainedImage);
  rootRow = static_cast<loka::app::StackNode *>(findNode(main, "SimpleViewer.RootRow"));
  RowSeatRecorder closed;
  recordRootSeats(rootRow, closed);
  LOKA_VERIFY(closed.navState.width == 0);
  LOKA_VERIFY(closed.contentState.x == 0);
  LOKA_VERIFY(closed.contentState.width == 600);

  SimpleViewerTestAccess::setNavMode(*main, simpleviewer::NAV_NARROW_OPEN);
  flushScene(*harness.scene);
  LOKA_VERIFY(findNode(main, "SimpleViewer.NavPane") != 0);
  LOKA_VERIFY(findNode(main, "SimpleViewer.NavToggle") != 0);
  LOKA_VERIFY(findNode(main, "SimpleViewer.Image") == retainedImage);
  rootRow = static_cast<loka::app::StackNode *>(findNode(main, "SimpleViewer.RootRow"));
  RowSeatRecorder open;
  recordRootSeats(rootRow, open);
  LOKA_VERIFY(open.navState.width == 200);
  LOKA_VERIFY(open.contentState.x == 200 + kRowGap);
}

void testSimpleViewerDisplayArmsAndMenuChecksFollowOwnedMode()
{
  SimpleViewerHarness harness;
  simpleviewer::MainNode *main = harness.mainNode();
  LOKA_VERIFY(main != 0);
  loka::app::ImageViewNode *image = static_cast<loka::app::ImageViewNode *>(
      findNode(main, "SimpleViewer.Image"));
  LOKA_VERIFY(image != 0);
  LOKA_VERIFY(image->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT);
  LOKA_VERIFY(findNode(main, "SimpleViewer.ActualScroll") == 0);
  verifyCheckedMode(viewMenu(harness.menuController.defaultMenuBar()),
                    simpleviewer::DISPLAY_FIT);

  SimpleViewerTestAccess::setDisplayMode(harness.config,
                                         simpleviewer::DISPLAY_ACTUAL);
  flushScene(*harness.scene);
  LOKA_VERIFY(harness.menuController.flushInvalidation(0));
  image = static_cast<loka::app::ImageViewNode *>(findNode(main, "SimpleViewer.Image"));
  LOKA_VERIFY(image != 0);
  LOKA_VERIFY(image->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_INTRINSIC);
  verifyCheckedMode(viewMenu(harness.menuController.defaultMenuBar()),
                    simpleviewer::DISPLAY_ACTUAL);

  SimpleViewerTestAccess::setDisplayMode(harness.config,
                                         simpleviewer::DISPLAY_ACTUAL_SCROLL);
  flushScene(*harness.scene);
  LOKA_VERIFY(harness.menuController.flushInvalidation(0));
  loka::app::ScrollViewNode *scroll = static_cast<loka::app::ScrollViewNode *>(
      findNode(main, "SimpleViewer.ActualScroll"));
  LOKA_VERIFY(scroll != 0);
  image = static_cast<loka::app::ImageViewNode *>(findNode(main, "SimpleViewer.Image"));
  LOKA_VERIFY(image != 0);
  LOKA_VERIFY(scroll->childrenHead() == image);
  LOKA_VERIFY(image->nextInComposition == 0);
  LOKA_VERIFY(image->props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_INTRINSIC);
  verifyCheckedMode(viewMenu(harness.menuController.defaultMenuBar()),
                    simpleviewer::DISPLAY_ACTUAL_SCROLL);
}

void testSimpleViewerDisplayModesProjectExpectedNullGeometry()
{
  SimpleViewerHarness harness;
  simpleviewer::MainNode *main = harness.mainNode();
  LOKA_VERIFY(main != 0);
  const loka::core::resource::Image fixture =
      loka::core::resource::Image::FromNative(reinterpret_cast<void *>(1),
                                              80,
                                              60,
                                              0,
                                              0);
  SimpleViewerTestAccess::setLoadedImage(*main, fixture);
  flushScene(*harness.scene);

  loka::app::StackNode *content = static_cast<loka::app::StackNode *>(
      findNode(main, "SimpleViewer.Content"));
  LOKA_VERIFY(content != 0);
  loka::app::scene::LayoutState contentState;
  contentState.x = 30;
  contentState.y = 40;
  contentState.width = 320;
  contentState.height = 240;
  contentState.lineHeight = 10;
  contentState.spacing = 4;

  loka::app::ImageViewNode *image = static_cast<loka::app::ImageViewNode *>(
      findNode(main, "SimpleViewer.Image"));
  LOKA_VERIFY(image != 0);
  harness.platform.projectLayoutForTesting(content, contentState);
  NullImageGeometryContext *imageContext =
      static_cast<NullImageGeometryContext *>(image->getContext());
  LOKA_VERIFY(imageContext != 0);
  const loka::app::scene::LayoutState *geometry = &imageContext->geometry();
  LOKA_VERIFY(geometry->x == 30);
  LOKA_VERIFY(geometry->y == 40);
  LOKA_VERIFY(geometry->width == 320);
  LOKA_VERIFY(geometry->height == 240);

  SimpleViewerTestAccess::setDisplayMode(harness.config,
                                         simpleviewer::DISPLAY_ACTUAL);
  flushScene(*harness.scene);
  image = static_cast<loka::app::ImageViewNode *>(
      findNode(main, "SimpleViewer.Image"));
  LOKA_VERIFY(image != 0);
  harness.platform.projectLayoutForTesting(content, contentState);
  imageContext = static_cast<NullImageGeometryContext *>(image->getContext());
  LOKA_VERIFY(imageContext != 0);
  geometry = &imageContext->geometry();
  LOKA_VERIFY(geometry->x == 30);
  LOKA_VERIFY(geometry->y == 40);
  LOKA_VERIFY(geometry->width == 80);
  LOKA_VERIFY(geometry->height == 60);
  loka::app::scene::Node *displaySeat = content->childrenHead();
  LOKA_VERIFY(displaySeat != 0);
  displaySeat = displaySeat->nextInComposition;
  LOKA_VERIFY(displaySeat != 0);
  loka::app::scene::INestable *displayNestable = displaySeat->asNestable();
  LOKA_VERIFY(displayNestable != 0);
  LOKA_VERIFY(displayNestable->childrenHead() == image);

  SimpleViewerTestAccess::setDisplayMode(harness.config,
                                         simpleviewer::DISPLAY_ACTUAL_SCROLL);
  flushScene(*harness.scene);
  loka::app::ScrollViewNode *scroll = static_cast<loka::app::ScrollViewNode *>(
      findNode(main, "SimpleViewer.ActualScroll"));
  image = static_cast<loka::app::ImageViewNode *>(
      findNode(main, "SimpleViewer.Image"));
  LOKA_VERIFY(scroll != 0);
  LOKA_VERIFY(image != 0);
  LOKA_VERIFY(scroll->childrenHead() == image);
  harness.platform.projectLayoutForTesting(content, contentState);
  imageContext = static_cast<NullImageGeometryContext *>(image->getContext());
  LOKA_VERIFY(imageContext != 0);
  geometry = &imageContext->geometry();
  LOKA_VERIFY(geometry->x == 30);
  LOKA_VERIFY(geometry->y == 40);
  LOKA_VERIFY(geometry->width == 80);
  LOKA_VERIFY(geometry->height == 60);
}

void testSimpleViewerPaneScrollButtonUsesMenuEmitter()
{
  SimpleViewerHarness harness;
  simpleviewer::MainNode *main = harness.mainNode();
  LOKA_VERIFY(main != 0);
  loka::app::ButtonNode *button = static_cast<loka::app::ButtonNode *>(
      findNode(main, "SimpleViewer.Mode.ActualScroll"));
  LOKA_VERIFY(button != 0);
  LOKA_VERIFY(button->props.onClick_ ==
              SimpleViewerTestAccess::actualScrollEvent(harness.config));

  button->props.onClick_->emit();
  flushScene(*harness.scene);
  LOKA_VERIFY(harness.menuController.flushInvalidation(0));
  LOKA_VERIFY(SimpleViewerTestAccess::displayModeState(harness.config)->get() ==
              simpleviewer::DISPLAY_ACTUAL_SCROLL);
  verifyCheckedMode(viewMenu(harness.menuController.defaultMenuBar()),
                    simpleviewer::DISPLAY_ACTUAL_SCROLL);
}

void testSimpleViewerNarrowWindowFileMenuMaterializesDialogOutsideParkedNav()
{
  NullPlatformContext platformContext;
  SimpleViewerAppConfig config(&platformContext);
  NullScenePlatformController platform;
  simpleviewer::MainProps mainProps;
  mainProps.platformContext(&platformContext)
      .openDialogEvent(SimpleViewerTestAccess::openDialogEvent(config))
      .displayMode(SimpleViewerTestAccess::displayModeState(config))
      .fitEvent(SimpleViewerTestAccess::fitEvent(config))
      .actualEvent(SimpleViewerTestAccess::actualEvent(config))
      .actualScrollEvent(SimpleViewerTestAccess::actualScrollEvent(config));
  loka::app::scene::NodeDefinitionBase *rootDefinition =
      loka::app::scene::Boundary<simpleviewer::MainNode>(mainProps).clone();
  LOKA_VERIFY(rootDefinition != 0);

  WindowProps windowProps;
  windowProps.frame(40, 40, 320, 240)
      .scene(new loka::app::scene::Scene(rootDefinition));
  NullWindow window(&platformContext, windowProps, &platform);
  LOKA_VERIFY(window.scene() != 0);
  loka::app::testing::WindowTestAccess::storeNativeFrame(
      window, loka::core::Frame(40, 40, 320, 240));
  window.scene()->updateAttached(true);
  if (window.hasPendingSceneInvalidation())
  {
    LOKA_VERIFY(window.flushSceneInvalidation());
  }

  simpleviewer::MainNode *main = static_cast<simpleviewer::MainNode *>(
      loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()));
  LOKA_VERIFY(main != 0);
  LOKA_VERIFY(findNode(main, "SimpleViewer.NavPane") == 0);
  LOKA_VERIFY(findNode(main, "SimpleViewer.NavToggle") != 0);
  LOKA_VERIFY(findNode(main, "SimpleViewerOpenFileDialog") == 0);

  SimpleViewerTestAccess::openDialogEvent(config)->emit();
  window.flushSceneInvalidation();
  LOKA_VERIFY(findNode(main, "SimpleViewerOpenFileDialog") != 0);
}
