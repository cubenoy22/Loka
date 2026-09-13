#ifndef LOKA_SMIRK_BENCH_MAIN_NODE_HPP
#define LOKA_SMIRK_BENCH_MAIN_NODE_HPP

#include "app/scene/BorrowedKeys.hpp"

#include "SmirkModel.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/String.hpp"
#include "core/util/StateTrackerGuard.hpp"

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
    {
      this->keys_.set(KEY_MODEL, model);
    }

    SmirkModel *model() const
    {
      return static_cast<SmirkModel *>(const_cast<void *>(this->keys_.get(KEY_MODEL)));
    }

    void assertInitialized() const
    {
      assert(this->keys_.complete());
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() &&
             this->keys_ < static_cast<const MainProps &>(rhs).keys_;
    }

  private:
    enum
    {
      KEY_MODEL,
      KEY_COUNT
    };
    loka::app::scene::BorrowedKeys<KEY_COUNT> keys_;
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
          navAxis_(),
          panelsAxis_(),
          navWidth_(),
          faceCount_(),
          surfaceExtent_(),
          faceCountText_(),
          addEnabled_(),
          addFace_()
    {
      const int initialFaceCount = props.model() ? props.model()->faceCount() : 0;
      this->state(this->orientation_, ORIENTATION_LANDSCAPE);
      this->state(this->navAxis_, loka::app::STACK_AXIS_COLUMN);
      this->state(this->panelsAxis_, loka::app::STACK_AXIS_ROW);
      this->state(this->navWidth_, int(kNavWidth));
      this->state(this->faceCount_, initialFaceCount);
      this->state(this->surfaceExtent_, loka::core::Frame());
      this->state(this->faceCountText_, this->faceCountLabel(initialFaceCount));
      this->state(this->addEnabled_, initialFaceCount < loka::app::RectSurfaceModel::kMaxRects);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      using namespace loka::app;
      this->props.assertInitialized();

      // Landscape: a fixed-width Row seat (#576 width claim). Portrait: an
      // unsized Box wraps the nav row, so the Column hands the surface the
      // remaining height below it.
      BoxDefinition navSeat = Box().width(this->navWidth_.state());
      navSeat.tag(kNavSeatTag);
      navSeat << (Stack(this->navAxis_.state()).TEST_ID("SmirkBench.NavPane")
                  << Button("Add face", &this->addFace_).enabled(this->addEnabled_.state()).TEST_ID("SmirkBench.AddFace")
                  << Text(this->faceCountText_.state()).TEST_ID("SmirkBench.FaceCount"));

      RectSurface surface = RectSurface(this->props.model()->surfaceModel())
                                .laidOutExtent(this->surfaceExtent_)
                                .useRegionClip(false)
                                .TEST_ID("SmirkBench.Surface");
      surface.tag(kSurfaceSeatTag);

      // Values reach the retained seats through their State props.
      StackDefinition panels = Stack(this->panelsAxis_.state()).TEST_ID("SmirkBench.Panels");
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

  private:
    static loka::core::String faceCountLabel(int count)
    {
      return loka::core::String::Literal("Faces: ") + loka::core::String::FromInt(count);
    }

    virtual void declareBindings(loka::app::scene::BindingToken &t)
    {
      t.action(this->addFace_, this, &MainNode::addFace);
      t.watch(*this->surfaceExtent_.state(), this, &MainNode::refreshModelBounds);
      ::Window *window = this->windowOrNull();
      if (window)
      {
        t.watch(window->nativeFrame(), this, &MainNode::refreshOrientation, true);
      }
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *context = this->attachedContext();
      return context ? context->window() : 0;
    }

    void addFace()
    {
      if (!this->props.model() || !this->props.model()->addFace())
      {
        return;
      }
      const int count = this->props.model()->faceCount();
      if (this->faceCount_.get() != count)
      {
        this->faceCount_.set(count);
      }
      this->faceCountText_.set(this->faceCountLabel(count));
      const bool enabled = this->props.model()->canAddFace();
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
      loka::core::StateTrackerGuard guard(this->tracker());
      this->orientation_.set(orientation);
      this->navAxis_.set(portrait ? loka::app::STACK_AXIS_ROW : loka::app::STACK_AXIS_COLUMN);
      this->panelsAxis_.set(portrait ? loka::app::STACK_AXIS_COLUMN : loka::app::STACK_AXIS_ROW);
      this->navWidth_.set(portrait ? 0 : int(kNavWidth));
    }

    void refreshModelBounds()
    {
      const loka::core::Frame frame = this->surfaceExtent_.get();
      this->props.model()->updateBounds(frame.width, frame.height);
    }

    loka::app::scene::NodeState<Orientation> orientation_;
    loka::app::scene::NodeState<loka::app::StackAxis> navAxis_;
    loka::app::scene::NodeState<loka::app::StackAxis> panelsAxis_;
    loka::app::scene::NodeState<int> navWidth_;
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
