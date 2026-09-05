#ifndef LOKA_SMIRK_BENCH_MAIN_NODE_HPP
#define LOKA_SMIRK_BENCH_MAIN_NODE_HPP

#include "SmirkModel.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/String.hpp"

namespace smirkbench
{
  /** The window's shape decides the axes: a landscape window seats the nav
      pane as a fixed-width column beside the surface, a portrait window
      seats it as a row above the surface. Nothing folds or hides; the
      containers only change axis (HW-1, #556). */
  enum Orientation
  {
    ORIENTATION_LANDSCAPE = 0,
    ORIENTATION_PORTRAIT = 1
  };

  enum
  {
    kNavWidth = 200
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
      kPanelsTag = 1,
      kNavSeatTag = 2,
      kSurfaceSeatTag = 3
    };

  public:
    typedef MainTypeTag TypeTag;

    explicit MainNode(const MainProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(props),
          orientation_(),
          faceCount_(),
          surfaceExtent_(),
          faceCountText_(),
          addEnabled_(),
          addFace_()
    {
      const int initialFaceCount = props.model_ ? props.model_->faceCount() : 0;
      this->state(this->orientation_, ORIENTATION_LANDSCAPE);
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
      const bool landscape = this->orientation_.get() == ORIENTATION_LANDSCAPE;

      // Landscape: a fixed-width Row seat (#576 width claim). Portrait: an
      // unsized Box wraps the nav row, so the Column hands the surface the
      // remaining height below it.
      BoxDefinition navSeat = Box().size(landscape ? kNavWidth : 0, 0);
      navSeat.tag(kNavSeatTag);
      navSeat << (Stack(landscape ? STACK_AXIS_COLUMN : STACK_AXIS_ROW).TEST_ID("SmirkBench.NavPane")
                  << Button("Add face", &this->addFace_).enabled(this->addEnabled_.state()).TEST_ID("SmirkBench.AddFace")
                  << Text(this->faceCountText_.state()).TEST_ID("SmirkBench.FaceCount"));

      RectSurface surface = RectSurface(this->props.model_->surfaceModel())
                                .laidOutExtent(this->surfaceExtent_)
                                .useRegionClip(false)
                                .TEST_ID("SmirkBench.Surface");
      surface.tag(kSurfaceSeatTag);

      // The root stays put; only the panels Stack below it flips its axis, so
      // the local recompose diffs the retained Stack's props (HelloWorld
      // keeps its flipping Stack under a constant root the same way).
      StackDefinition panels = Stack(landscape ? STACK_AXIS_ROW : STACK_AXIS_COLUMN).TEST_ID("SmirkBench.Panels");
      panels.tag(kPanelsTag);
      panels << navSeat << surface;
      composition.declare(Box().TEST_ID("SmirkBench.Root") << panels);
    }

#if defined(TEST_BUILD)
    /** Scenario-test door: derives the orientation without requiring a Window. */
    void refreshOrientationForTesting(const loka::core::Frame &frame)
    {
      this->refreshOrientationFromFrame(frame);
    }

    Orientation orientationForTesting() const
    {
      return this->orientation_.get();
    }

    loka::core::Frame surfaceExtentForTesting() const
    {
      return this->surfaceExtent_.get();
    }

    /** Test-only tracker observation proving identical frames do not write. */
    bool consumeOrientationTrackerDirtForTesting()
    {
      loka::core::StateTracker *tracker = this->orientation_.dangerouslyTracker();
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
        // The orientation flip changes the retained Stacks' axis props, not
        // the structure; the diff apply carries props (HelloWorld, #556).
        this->recomposeLocalCompositionWithFullFallback(
            context, event, this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS);
        this->bindUi();
        return;
      }
      BaseType::composeWithContext(context, event);
    }

  private:
    static loka::core::String faceCountLabel(int count)
    {
      return loka::core::String::Literal("Faces: ") + loka::core::String::FromInt(count);
    }

    void bindUi()
    {
      this->bindActionForUi(this->addFace_, &MainNode::addFace);
      this->watchStateForUi(*this->surfaceExtent_.state(), &MainNode::refreshModelBounds);
      ::Window *window = this->windowOrNull();
      if (window)
      {
        this->watchStateForUi(window->nativeFrame(), &MainNode::refreshOrientation, true);
      }
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *context = this->attachedContext();
      return context ? context->window() : 0;
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

    void refreshOrientation()
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      this->refreshOrientationFromFrame(window->nativeFrame().get());
    }

    /** A window taller than it is wide is portrait; an unsized frame keeps
        the landscape default. */
    void refreshOrientationFromFrame(const loka::core::Frame &frame)
    {
      const bool portrait = frame.width > 0 && frame.height > frame.width;
      const Orientation orientation = portrait ? ORIENTATION_PORTRAIT : ORIENTATION_LANDSCAPE;
      if (this->orientation_.get() == orientation)
      {
        return;
      }
      this->orientation_.set(orientation);
      // No node subscribes to the orientation; the boundary recomposes on
      // its own CHILD dirt (HelloWorld's flip, #556).
      this->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
    }

    void refreshModelBounds()
    {
      const loka::core::Frame frame = this->surfaceExtent_.get();
      this->props.model_->updateBounds(frame.width, frame.height);
    }

    loka::app::scene::NodeState<Orientation> orientation_;
    loka::app::scene::NodeState<int> faceCount_;
    /** The model's bounce walls equal the last laid-out seat the rail delivered for the surface; until the first
        delivery they are the constructor's bounds. */
    loka::app::scene::NodeState<loka::core::Frame> surfaceExtent_;
    loka::app::scene::NodeState<loka::core::String> faceCountText_;
    loka::app::scene::NodeState<bool> addEnabled_;
    loka::core::EmitterState addFace_;
  };
} // namespace smirkbench

#endif // LOKA_SMIRK_BENCH_MAIN_NODE_HPP
