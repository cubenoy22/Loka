#ifndef LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
#define LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Match.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/core/Window.hpp"
#include "ImageLoadSession.hpp"
#include "core/State.hpp"
#include "core/String.hpp"
#include "core/resource/Image.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include <cassert>

#ifdef TEST_BUILD
class SimpleViewerTestAccess;
#endif

namespace simpleviewer
{
  using loka::app::Button;

  enum NavMode
  {
    NAV_WIDE = 0,
    NAV_NARROW_CLOSED = 1,
    NAV_NARROW_OPEN = 2
  };

  enum DisplayMode
  {
    DISPLAY_FIT = 0,
    DISPLAY_ACTUAL = 1,
    DISPLAY_ACTUAL_SCROLL = 2
  };

  class MainTypeTag
  {
  };

  class MainNode;

  struct MainProps : public loka::app::scene::NodePropsBase<MainProps>
  {
    typedef MainTypeTag TypeTag;
    typedef MainNode NodeType;
    PlatformContext *platformContext_;
    loka::core::EmitterState *openDialogEvent_;
    loka::core::State<DisplayMode> *displayMode_;
    loka::core::EmitterState *fitEvent_;
    loka::core::EmitterState *actualEvent_;
    loka::core::EmitterState *actualScrollEvent_;

    MainProps()
        : platformContext_(0),
          openDialogEvent_(0),
          displayMode_(0),
          fitEvent_(0),
          actualEvent_(0),
          actualScrollEvent_(0)
    {
    }

    MainProps &platformContext(PlatformContext *context)
    {
      this->platformContext_ = context;
      return *this;
    }

    MainProps &openDialogEvent(loka::core::EmitterState *eventState)
    {
      this->openDialogEvent_ = eventState;
      return *this;
    }

    MainProps &displayMode(loka::core::State<DisplayMode> *state)
    {
      this->displayMode_ = state;
      return *this;
    }

    MainProps &fitEvent(loka::core::EmitterState *eventState)
    {
      this->fitEvent_ = eventState;
      return *this;
    }

    MainProps &actualEvent(loka::core::EmitterState *eventState)
    {
      this->actualEvent_ = eventState;
      return *this;
    }

    MainProps &actualScrollEvent(loka::core::EmitterState *eventState)
    {
      this->actualScrollEvent_ = eventState;
      return *this;
    }

    void assertInitialized() const
    {
      assert(this->platformContext_);
      assert(this->openDialogEvent_);
      assert(this->displayMode_);
      assert(this->fitEvent_);
      assert(this->actualEvent_);
      assert(this->actualScrollEvent_);
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      if (this->platformContext_ != other.platformContext_)
        return this->platformContext_ < other.platformContext_;
      if (this->openDialogEvent_ != other.openDialogEvent_)
        return this->openDialogEvent_ < other.openDialogEvent_;
      if (this->displayMode_ != other.displayMode_)
        return this->displayMode_ < other.displayMode_;
      if (this->fitEvent_ != other.fitEvent_)
        return this->fitEvent_ < other.fitEvent_;
      if (this->actualEvent_ != other.actualEvent_)
        return this->actualEvent_ < other.actualEvent_;
      if (this->actualScrollEvent_ != other.actualScrollEvent_)
        return this->actualScrollEvent_ < other.actualScrollEvent_;
      return false;
    }
  };

  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
    enum
    {
      kRootRowTag = 1,
      kNavSeatTag = 2,
      kContentTag = 3,
      kNavToggleSeatTag = 4,
      kDisplaySeatTag = 5,
      kImageViewTag = 6,
      kOpenDialogTag = 7,
      kRootTag = 8
    };

    static const int kNavWidth = 200;
    static const int kNarrowBreakpoint = 480;

  public:
    typedef MainTypeTag TypeTag;

    MainNode(const MainProps &p)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(p),
          isDialogShown_(),
          chooserResult_(),
          chooserMessage_(),
          image_(),
          navMode_(),
          navOpen_(),
          scrollOffset_(),
          toggleNavEvent_(),
          imageLoad_()
    {
      this->state(this->isDialogShown_, false);
      this->state(this->chooserResult_, loka::app::FileChooserResult());
      this->state(this->chooserMessage_, loka::core::String::Literal("(none)"));
      this->state(this->image_, loka::core::resource::Image::Empty());
      this->state(this->navMode_, static_cast<int>(NAV_WIDE));
      this->state(this->navOpen_, false);
      this->state(this->scrollOffset_, 0);
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      this->bindUi();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      this->props.assertInitialized();

      MatchDefinition<int> nav = Match(*this->navMode_.state());
      nav.arm(NAV_WIDE, this->navPane(false))
          .arm(NAV_NARROW_OPEN, this->navPane(true))
          .otherwise(Fragment());
      nav.setNodeTag(kNavSeatTag);

      MatchDefinition<int> navToggle = Match(*this->navMode_.state());
      navToggle.arm(NAV_NARROW_CLOSED, this->navToggleButton())
          .otherwise(Fragment());
      navToggle.setNodeTag(kNavToggleSeatTag);

      MatchDefinition<DisplayMode> display = Match(*this->props.displayMode_);
      display.arm(DISPLAY_FIT, this->imageView(IMAGE_VIEW_SIZE_FILL_PARENT))
          .arm(DISPLAY_ACTUAL, this->imageView(IMAGE_VIEW_SIZE_INTRINSIC))
          .arm(DISPLAY_ACTUAL_SCROLL,
               ScrollView(this->scrollOffset_)
                       .TEST_ID("SimpleViewer.ActualScroll")
               << this->imageView(IMAGE_VIEW_SIZE_INTRINSIC));
      display.setNodeTag(kDisplaySeatTag);

      ShowDefinition openDialog =
          Show(*this->isDialogShown_.state())
          << (PolicyScopeDefinition().destroyOnDetach()
              << OpenFileDialog().result(this->chooserResult_).testId("SimpleViewerOpenFileDialog"));
      openDialog.setNodeTag(kOpenDialogTag);

      HStack rootRow = HStack().TEST_ID("SimpleViewer.RootRow");
      rootRow.setNodeTag(kRootRowTag);
      VStack content = VStack().TEST_ID("SimpleViewer.Content");
      content.setNodeTag(kContentTag);
      content << navToggle << display;
      rootRow << nav << content;
      VStack root = VStack().TEST_ID("SimpleViewer.Root");
      root.setNodeTag(kRootTag);
      c.declare(root << rootRow << openDialog);
    }

  protected:
    virtual void declareLocalRecomposition(loka::app::scene::NodeComposition &composition)
    {
      this->composeNode(composition);
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::StdCompositionBoundaryNodeBase<MainProps> BaseType;
      if (event == loka::app::scene::COMPOSE_EVENT_UPDATE &&
          (context.dirtyFlags() & loka::app::scene::NODE_DIRTY_CHILD))
      {
        this->recomposeLocalCompositionWithFullFallback(
            context, event, this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS);
        this->bindUi();
        return;
      }
      BaseType::composeWithContext(context, event);
    }

  private:
    friend class ImageLoadSession;
#ifdef TEST_BUILD
    friend class ::SimpleViewerTestAccess;
#endif

    loka::app::ButtonDefinition navToggleButton()
    {
      using namespace loka::app;
      return Button("=")
          .onClick(&this->toggleNavEvent_)
          .TEST_ID("SimpleViewer.NavToggle");
    }

    loka::app::HStack navToggleHeader()
    {
      using namespace loka::app;
      return HStack()
             << (Box().size(layout::FallbackControlMetrics::kButtonHeight,
                            layout::FallbackControlMetrics::kButtonHeight)
                 << this->navToggleButton());
    }

    loka::app::Box navPane(bool showToggle)
    {
      using namespace loka::app;
      VStack contents = VStack().alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING);
      if (showToggle)
      {
        contents << this->navToggleHeader();
      }
      contents << Button("Open...").onClick(this->props.openDialogEvent_)
               << Text("Loka file:")
               << Text(this->chooserMessage_.state())
                      .attr(TextAttr().wrap(TEXT_WRAP_CHAR).truncation(TEXT_TRUNCATION_NONE))
               << Button("Fit to Window")
                      .onClick(this->props.fitEvent_)
                      .TEST_ID("SimpleViewer.Mode.Fit")
               << Button("Actual Size")
                      .onClick(this->props.actualEvent_)
                      .TEST_ID("SimpleViewer.Mode.Actual")
               << Button("Actual Size (Scroll)")
                      .onClick(this->props.actualScrollEvent_)
                      .TEST_ID("SimpleViewer.Mode.ActualScroll");
      return Box()
                 .size(kNavWidth, 0)
                 .TEST_ID("SimpleViewer.NavPane")
             << contents;
    }

    loka::app::ImageViewDefinitionWithAttr imageView(
        loka::app::ImageViewSizePolicy sizePolicy)
    {
      using namespace loka::app;
      ImageViewDefinitionWithAttr image =
          ImageView()
              .image(this->image_.state())
              .attr(ImageViewAttr().sizePolicy(sizePolicy).fit(IMAGE_FIT_CONTAIN))
              .TEST_ID("SimpleViewer.Image");
      image.setNodeTag(kImageViewTag);
      return image;
    }

    void bindUi()
    {
      this->bindActionForUi(*this->props.openDialogEvent_, &MainNode::openDialog);
      this->bindActionForUi(this->toggleNavEvent_, &MainNode::toggleNavigation);
      this->watchStateForUi(*this->props.displayMode_, &MainNode::refreshDisplayMode);
      ::Window *window = this->windowOrNull();
      if (window)
      {
        this->watchStateForUi(window->nativeFrame(), &MainNode::refreshLayoutMode, true);
      }
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *ctx = this->attachedContext();
      return ctx ? ctx->window() : 0;
    }

    void refreshLayoutMode()
    {
      ::Window *window = this->windowOrNull();
      if (!window || !this->navMode_.isValid())
      {
        return;
      }
      const loka::core::Frame frame = window->nativeFrame().get();
      const bool narrow = frame.hasSize() && frame.width > 0 && frame.width < kNarrowBreakpoint;
      const int mode = narrow
                           ? (this->navOpen_.get() ? NAV_NARROW_OPEN : NAV_NARROW_CLOSED)
                           : NAV_WIDE;
      if (this->navMode_.get() == mode)
      {
        return;
      }
      this->navMode_.set(mode);
      this->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
    }

    void toggleNavigation()
    {
      this->navOpen_.set(!this->navOpen_.get());
      this->refreshLayoutMode();
    }

    void refreshDisplayMode()
    {
      this->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
    }

    void openDialog()
    {
      this->imageLoad_.begin(
          *this,
          this->props.platformContext_,
          this->chooserResult_.state(),
          static_cast<loka::core::PushStateTracker *>(this->tracker()));
      this->isDialogShown_.set(true, true);
    }

    bool hasCurrentImage() const
    {
      return this->image_.get().isValid();
    }

    void releaseCurrentImageForLoad()
    {
      const loka::core::resource::Image empty = loka::core::resource::Image::Empty();
      if (this->image_.get() == empty)
      {
        return;
      }
      this->image_.set(empty);
    }

    void commitLoadedImage(const loka::core::resource::Image &image)
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->scrollOffset_.set(0);
      this->image_.set(image, true);
    }

    void closeDialogForChooserResult(const loka::app::FileChooserResult &result)
    {
      if (result.kind != loka::app::FileChooserResult::RESULT_NONE && this->isDialogShown_.get())
      {
        this->isDialogShown_.set(false, true);
      }
    }

    bool isImageLoadDialogShown() const
    {
      return this->isDialogShown_.get();
    }

    void setChooserMessageIfChanged(const loka::core::String &message)
    {
      if (this->chooserMessage_.get().equals(message))
      {
        return;
      }
      this->chooserMessage_.set(message);
    }

    loka::app::scene::NodeState<bool> isDialogShown_;
    loka::app::scene::NodeState<loka::app::FileChooserResult> chooserResult_;
    loka::app::scene::NodeState<loka::core::String> chooserMessage_;
    loka::app::scene::NodeState<loka::core::resource::Image> image_;
    loka::app::scene::NodeState<int> navMode_;
    loka::app::scene::NodeState<bool> navOpen_;
    loka::app::scene::NodeState<int> scrollOffset_;
    loka::core::EmitterState toggleNavEvent_;
    ImageLoadSession imageLoad_;
  };
} // namespace simpleviewer

#include "ImageLoadSessionFlow.hpp"

#endif // LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
