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
  public:
    typedef MainTypeTag TypeTag;

    explicit MainNode(const MainProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(props),
          navMode_(),
          navOpen_(),
          faceCount_(),
          faceCountText_(),
          addEnabled_(),
          navToggle_(),
          addFace_()
    {
      const int initialFaceCount = props.model_ ? props.model_->faceCount() : 0;
      this->state(this->navMode_, NAV_WIDE);
      this->state(this->navOpen_, false);
      this->state(this->faceCount_, initialFaceCount);
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
      composition.declare(
          Row().TEST_ID("SmirkBench.Root")
          << Match(*this->navMode_.state())
                 .arm(NAV_WIDE, Box().size(kNavWidth, 0) << this->navPane())
                 .arm(NAV_NARROW_OPEN, Box().size(kNavWidth, 0) << this->navPane())
                 .otherwise(Fragment())
          << (Column()
              << Match(*this->navMode_.state())
                     .arm(&MainNode::isNarrowMode, 0, Button("=", &this->navToggle_).TEST_ID("SmirkBench.NavToggle"))
                     .otherwise(Fragment())
              << RectSurface(this->props.model_->surfaceModel()).useRegionClip(false).TEST_ID("SmirkBench.Surface")));
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

    /** Test-only tracker observation proving identical frames do not write. */
    bool consumeNavModeTrackerDirtForTesting()
    {
      loka::core::StateTracker *tracker = this->navMode_.dangerouslyTracker();
      loka::core::PushStateTracker *pushTracker = tracker ? tracker->asPushTracker() : 0;
      return pushTracker ? pushTracker->consumeDirty() : false;
    }
#endif

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

      if (this->props.model_)
      {
        int surfaceWidth = frame.width;
        if (mode == NAV_WIDE || mode == NAV_NARROW_OPEN)
        {
          surfaceWidth -= kNavWidth;
        }
        if (surfaceWidth < 0)
        {
          surfaceWidth = 0;
        }
        this->props.model_->updateBounds(static_cast<short>(surfaceWidth), frame.height);
      }
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
    loka::app::scene::NodeState<loka::core::String> faceCountText_;
    loka::app::scene::NodeState<bool> addEnabled_;
    loka::core::EmitterState navToggle_;
    loka::core::EmitterState addFace_;
  };
} // namespace smirkbench

#endif // LOKA_SMIRK_BENCH_MAIN_NODE_HPP
