#ifndef LOKA_SMIRK_BENCH_MAIN_NODE_HPP
#define LOKA_SMIRK_BENCH_MAIN_NODE_HPP

#include "SmirkModel.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Match.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/String.hpp"

namespace smirkbench
{
  enum NavMode
  {
    NAV_WIDE = 0,
    NAV_NARROW_CLOSED = 1,
    NAV_NARROW_OPEN = 2
  };

  enum
  {
    kNavWidth = 200,
    kNavBreakpoint = 480
  };

  struct MainTypeTag
  {
  };

  class MainNode;

  struct MainProps : public loka::app::scene::NodePropsBase<MainProps>
  {
    typedef MainTypeTag TypeTag;
    typedef MainNode NodeType;

    explicit MainProps(SmirkModel *model = 0)
        : model_(model)
    {
    }

    void assertInitialized() const
    {
      assert(this->model_);
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      return this->model_ < other.model_;
    }

    SmirkModel *model_;
  };

  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
    enum
    {
      kNavSeatTag = 1,
      kContentSeatTag = 2,
      kToggleSeatTag = 3,
      kSurfaceSeatTag = 4
    };

  public:
    typedef MainTypeTag TypeTag;

    explicit MainNode(const MainProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(props),
          navMode_(),
          navOpen_(),
          faceCount_(),
          surfaceExtent_(),
          faceCountText_(),
          addEnabled_(),
          navToggle_(),
          addFace_()
    {
      const int initialFaceCount = props.model_ ? props.model_->faceCount() : 0;
      this->state(this->navMode_, NAV_WIDE);
      this->state(this->navOpen_, false);
      this->state(this->faceCount_, initialFaceCount);
      this->state(this->surfaceExtent_, loka::core::Frame());
      this->state(this->faceCountText_, this->faceCountLabel(initialFaceCount));
      this->state(this->addEnabled_, initialFaceCount < loka::app::RectSurfaceModel::kMaxRects);
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      this->bindUi();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      MatchDefinition<NavMode> nav = Match(*this->navMode_.state())
                                         .arm(NAV_WIDE, Box().size(kNavWidth, 0) << this->navPane())
                                         .arm(NAV_NARROW_OPEN, Box().size(kNavWidth, 0) << this->navPane())
                                         .otherwise(Fragment());
      nav.setNodeTag(kNavSeatTag);
      MatchDefinition<NavMode> toggle =
          Match(*this->navMode_.state())
              .arm(&MainNode::isNarrowMode, 0, Button("=", &this->navToggle_).TEST_ID("SmirkBench.NavToggle"))
              .otherwise(Fragment());
      toggle.setNodeTag(kToggleSeatTag);
      RectSurface surface = RectSurface(this->props.model_->surfaceModel())
                                .laidOutExtent(this->surfaceExtent_)
                                .useRegionClip(false)
                                .TEST_ID("SmirkBench.Surface");
      surface.tag(kSurfaceSeatTag);
      Column content;
      content.tag(kContentSeatTag);
      content << toggle << surface;
      composition.declare(Row().TEST_ID("SmirkBench.Root") << nav << content);
    }

#if defined(TEST_BUILD)
    /** Scenario-test door: drives Match directly without requiring a Window. */
    void setNavModeForTesting(NavMode mode)
    {
      this->applyNavMode(mode);
    }

    void setNavOpenForTesting(bool open)
    {
      if (this->navOpen_.get() != open)
      {
        this->navOpen_.set(open);
      }
    }

    void refreshNavModeForTesting(const loka::core::Frame &frame)
    {
      this->refreshNavModeFromFrame(frame);
    }

    NavMode navModeForTesting() const
    {
      return this->navMode_.get();
    }

    loka::core::Frame surfaceExtentForTesting() const
    {
      return this->surfaceExtent_.get();
    }

    /** Test-only tracker observation proving identical frames do not write. */
    bool consumeNavModeTrackerDirtForTesting()
    {
      loka::core::StateTracker *tracker = this->navMode_.dangerouslyTracker();
      loka::core::PushStateTracker *pushTracker = tracker ? tracker->asPushTracker() : 0;
      return pushTracker ? pushTracker->consumeDirty() : false;
    }
#endif

  protected:
    virtual void declareLocalRecomposition(loka::app::scene::NodeComposition &composition)
    {
      this->composeNode(composition);
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context, loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::StdCompositionBoundaryNodeBase<MainProps> BaseType;
      if (event == loka::app::scene::COMPOSE_EVENT_UPDATE
          && (context.dirtyFlags() & loka::app::scene::NODE_DIRTY_CHILD))
      {
        if (!this->recomposeLocalComposition(context, event, this->LOCAL_RECOMPOSE_APPLY_SNAPSHOT)
            && !this->composeResult().allocationFailed)
        {
          this->recomposeLocalCompositionWithFullFallback(
              context, event, this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS);
        }
        this->bindUi();
        return;
      }
      BaseType::composeWithContext(context, event);
    }

  private:
    static bool isNarrowMode(const NavMode &mode, void *)
    {
      return mode == NAV_NARROW_CLOSED || mode == NAV_NARROW_OPEN;
    }

    loka::app::Column navPane()
    {
      using namespace loka::app;
      return Column().TEST_ID("SmirkBench.NavPane")
             << Button("Add face", &this->addFace_).enabled(this->addEnabled_.state()).TEST_ID("SmirkBench.AddFace")
             << Text(this->faceCountText_.state()).TEST_ID("SmirkBench.FaceCount");
    }

    static loka::core::String faceCountLabel(int count)
    {
      return loka::core::String::Literal("Faces: ") + loka::core::String::FromInt(count);
    }

    void bindUi()
    {
      this->bindActionForUi(this->navToggle_, &MainNode::toggleNav);
      this->bindActionForUi(this->addFace_, &MainNode::addFace);
      this->watchStateForUi(*this->surfaceExtent_.state(), &MainNode::refreshModelBounds);
      ::Window *window = this->windowOrNull();
      if (window)
      {
        this->watchStateForUi(window->nativeFrame(), &MainNode::refreshNavMode, true);
      }
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *context = this->attachedContext();
      return context ? context->window() : 0;
    }

    void toggleNav()
    {
      if (!this->navOpen_.isValid())
      {
        return;
      }
      this->navOpen_.set(!this->navOpen_.get());
      this->refreshNavMode();
    }

    void addFace()
    {
      if (!this->props.model_ || !this->props.model_->addFace())
      {
        return;
      }
      const int count = this->props.model_->faceCount();
      if (this->faceCount_.get() != count)
      {
        this->faceCount_.set(count);
      }
      this->faceCountText_.set(this->faceCountLabel(count));
      const bool enabled = this->props.model_->canAddFace();
      if (this->addEnabled_.get() != enabled)
      {
        this->addEnabled_.set(enabled);
      }
    }

    void refreshNavMode()
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      this->refreshNavModeFromFrame(window->nativeFrame().get());
    }

    void refreshNavModeFromFrame(const loka::core::Frame &frame)
    {
      const bool narrow = frame.width < kNavBreakpoint;
      const NavMode mode = narrow ? (this->navOpen_.get() ? NAV_NARROW_OPEN : NAV_NARROW_CLOSED) : NAV_WIDE;
      if (this->navMode_.get() != mode)
      {
        this->navMode_.set(mode);
      }
    }

    void refreshModelBounds()
    {
      const loka::core::Frame frame = this->surfaceExtent_.get();
      this->props.model_->updateBounds(frame.width, frame.height);
    }

    void applyNavMode(NavMode mode)
    {
      if (this->navMode_.get() != mode)
      {
        this->navMode_.set(mode);
      }
      const bool open = mode == NAV_NARROW_OPEN;
      if (this->navOpen_.get() != open)
      {
        this->navOpen_.set(open);
      }
    }

    loka::app::scene::NodeState<NavMode> navMode_;
    loka::app::scene::NodeState<bool> navOpen_;
    loka::app::scene::NodeState<int> faceCount_;
    /** The model's bounce walls equal the last laid-out seat the rail delivered for the surface; until the first
        delivery they are the constructor's bounds. */
    loka::app::scene::NodeState<loka::core::Frame> surfaceExtent_;
    loka::app::scene::NodeState<loka::core::String> faceCountText_;
    loka::app::scene::NodeState<bool> addEnabled_;
    loka::core::EmitterState navToggle_;
    loka::core::EmitterState addFace_;
  };
} // namespace smirkbench

#endif // LOKA_SMIRK_BENCH_MAIN_NODE_HPP
