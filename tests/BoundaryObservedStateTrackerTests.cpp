#include "BoundaryObservedStateTrackerTests.hpp"

#include <cassert>
#include <cstdio>

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace
{
  class DoubleClockParentBoundaryNode;
  class DoubleClockChildBoundaryNode;

  struct DoubleClockTrace
  {
    DoubleClockTrace()
        : parent(0),
          child(0),
          observed(0),
          parentTracker(0),
          childTracker(0),
          ownerDuringParent(0),
          ownerDuringChild(0),
          ownerAfterChild(0),
          ownerBeforeSecondWrite(0)
    {
    }

    DoubleClockParentBoundaryNode *parent;
    DoubleClockChildBoundaryNode *child;
    loka::core::State<bool> *observed;
    loka::core::StateTracker *parentTracker;
    loka::core::StateTracker *childTracker;
    loka::core::StateTracker *ownerDuringParent;
    loka::core::StateTracker *ownerDuringChild;
    loka::core::StateTracker *ownerAfterChild;
    loka::core::StateTracker *ownerBeforeSecondWrite;
  };

  struct DoubleClockChildTypeTag
  {
  };

  struct DoubleClockChildProps
      : public loka::app::scene::NodePropsBase<DoubleClockChildProps>
  {
    typedef DoubleClockChildTypeTag TypeTag;
    typedef DoubleClockChildBoundaryNode NodeType;

    DoubleClockChildProps(loka::core::State<bool> *observedState = 0,
                          DoubleClockTrace *traceValue = 0)
        : observed(observedState),
          trace(traceValue)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const DoubleClockChildProps &other =
          static_cast<const DoubleClockChildProps &>(rhs);
      if (this->observed != other.observed)
      {
        return this->observed < other.observed;
      }
      return this->trace < other.trace;
    }

    loka::core::State<bool> *observed;
    DoubleClockTrace *trace;
  };

  void recordObservedOwnerDuringChildSettlement(void *userData)
  {
    DoubleClockTrace *trace = static_cast<DoubleClockTrace *>(userData);
    assert(trace);
    assert(trace->observed);
    trace->ownerDuringChild = trace->observed->trackerOwner();
    std::fprintf(stderr,
                 "double-clock stamp during child settlement: observed=%p owner=%p child=%p\n",
                 static_cast<void *>(trace->observed),
                 static_cast<void *>(trace->ownerDuringChild),
                 static_cast<void *>(trace->childTracker));
  }

  class DoubleClockChildBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<DoubleClockChildProps>
  {
  public:
    explicit DoubleClockChildBoundaryNode(const DoubleClockChildProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<DoubleClockChildProps>(props),
          local_()
    {
      this->state(this->local_, 0);
      if (this->props.trace)
      {
        this->props.trace->child = this;
        this->props.trace->childTracker = this->tracker();
      }
    }

    virtual ~DoubleClockChildBoundaryNode()
    {
      if (this->props.trace && this->props.trace->child == this)
      {
        this->props.trace->child = 0;
      }
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      this->local_.bind(&recordObservedOwnerDuringChildSettlement,
                        this->props.trace,
                        false);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::Button("observed-parent-state").enabled(this->props.observed));
    }

    void setLocal(int value)
    {
      this->local_.set(value);
    }

  private:
    loka::app::scene::NodeState<int> local_;
  };

  class ObservedAsIntEval : public loka::core::DerivedState<int>::EvalFn
  {
  public:
    explicit ObservedAsIntEval(loka::core::State<bool> *observed)
        : observed_(observed)
    {
    }

    virtual int operator()()
    {
      return this->observed_->get() ? 2 : 1;
    }

  private:
    loka::core::State<bool> *observed_;
  };

  struct DoubleClockParentTypeTag
  {
  };

  struct DoubleClockParentProps
      : public loka::app::scene::NodePropsBase<DoubleClockParentProps>
  {
    typedef DoubleClockParentTypeTag TypeTag;
    typedef DoubleClockParentBoundaryNode NodeType;

    explicit DoubleClockParentProps(DoubleClockTrace *traceValue = 0)
        : trace(traceValue)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const DoubleClockParentProps &other =
          static_cast<const DoubleClockParentProps &>(rhs);
      return this->trace < other.trace;
    }

    DoubleClockTrace *trace;
  };

  void setChildStateFromParentCommit(void *userData)
  {
    DoubleClockTrace *trace = static_cast<DoubleClockTrace *>(userData);
    assert(trace);
    assert(trace->child);
    assert(trace->observed);
    trace->child->setLocal(1);
    trace->ownerAfterChild = trace->observed->trackerOwner();
    std::fprintf(stderr,
                 "double-clock stamp after child end: observed=%p owner=%p\n",
                 static_cast<void *>(trace->observed),
                 static_cast<void *>(trace->ownerAfterChild));
  }

  void setObservedStateAgainFromParentCommit(void *userData);

  class DoubleClockParentBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<DoubleClockParentProps>
  {
  public:
    explicit DoubleClockParentBoundaryNode(const DoubleClockParentProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<DoubleClockParentProps>(props),
          observed_(),
          derived_(0)
    {
      this->state(this->observed_, false);
      if (this->props.trace)
      {
        this->props.trace->parent = this;
        this->props.trace->parentTracker = this->tracker();
      }
    }

    virtual ~DoubleClockParentBoundaryNode()
    {
      if (this->props.trace && this->props.trace->parent == this)
      {
        this->props.trace->parent = 0;
      }
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      if (!this->derived_)
      {
        this->derived_ = new loka::core::DerivedState<int>(
            this->observed_.state(),
            new ObservedAsIntEval(this->observed_.state()));
        this->adoptState(this->derived_);
      }
      if (this->props.trace)
      {
        this->props.trace->observed = this->observed_.state();
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::scene::Boundary<DoubleClockChildBoundaryNode>(
              DoubleClockChildProps(this->observed_.state(), this->props.trace)));
    }

    void runCommitChain()
    {
      assert(this->props.trace);
      {
        loka::core::StateTrackerGuard guard(this->tracker());
        this->props.trace->ownerDuringParent =
            this->observed_.state()->trackerOwner();
        std::fprintf(stderr,
                     "double-clock stamp in parent transaction: observed=%p owner=%p parent=%p\n",
                     static_cast<void *>(this->observed_.state()),
                     static_cast<void *>(this->props.trace->ownerDuringParent),
                     static_cast<void *>(this->tracker()));
        this->tracker()->defer(&setChildStateFromParentCommit, this->props.trace);
        this->tracker()->defer(&setObservedStateAgainFromParentCommit, this->props.trace);
      }
    }

    void setObserved(bool value)
    {
      this->observed_.set(value);
    }

    int derivedValue() const
    {
      assert(this->derived_);
      return this->derived_ ? this->derived_->get() : 0;
    }

    bool observedValue() const
    {
      return this->observed_.get();
    }

  private:
    loka::app::scene::NodeState<bool> observed_;
    loka::core::DerivedState<int> *derived_;
  };

  void setObservedStateAgainFromParentCommit(void *userData)
  {
    DoubleClockTrace *trace = static_cast<DoubleClockTrace *>(userData);
    assert(trace);
    assert(trace->parent);
    assert(trace->observed);
    trace->ownerBeforeSecondWrite = trace->observed->trackerOwner();
    std::fprintf(stderr,
                 "double-clock stamp before second parent write: observed=%p owner=%p\n",
                 static_cast<void *>(trace->observed),
                 static_cast<void *>(trace->ownerBeforeSecondWrite));
    trace->parent->setObserved(true);
  }

  class DoubleClockPlatformController
      : public loka::app::scene::IPlatformController
  {
  public:
    virtual void onChange(loka::app::scene::Node *,
                          loka::app::scene::NodeDirtyFlags,
                          bool)
    {
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}
  };
} // namespace

void testObservedStateDoesNotJoinChildBoundaryTracker()
{
  std::printf("\n==== [testObservedStateDoesNotJoinChildBoundaryTracker] start ====\n");
  DoubleClockTrace trace;
  DoubleClockPlatformController platform;
  loka::app::scene::Scene scene(
      loka::app::scene::Boundary<DoubleClockParentBoundaryNode>(
          DoubleClockParentProps(&trace)));
  scene.mount(&platform);
  scene.updateAttached(true);

  assert(trace.parent);
  assert(trace.child);
  assert(trace.observed);
  assert(trace.parentTracker);
  assert(trace.childTracker);
  assert(trace.parent->derivedValue() == 1);

  trace.parent->runCommitChain();

  std::fprintf(stderr,
               "double-clock result: observed=%d derived=%d expected=%d\n",
               trace.parent->observedValue() ? 1 : 0,
               trace.parent->derivedValue(),
               trace.parent->observedValue() ? 2 : 1);

  assert(trace.parent->derivedValue() ==
         (trace.parent->observedValue() ? 2 : 1));
  assert(trace.ownerDuringParent == trace.parentTracker);
  assert(trace.ownerDuringChild == trace.parentTracker);
  assert(trace.ownerAfterChild == trace.parentTracker);
  assert(trace.ownerBeforeSecondWrite == trace.parentTracker);

  scene.updateAttached(false);
  scene.unmount();
  std::printf("==== [testObservedStateDoesNotJoinChildBoundaryTracker] end ====\n");
}
