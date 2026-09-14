#include "DerivedNodeStateTests.hpp"
#include "support/TestVerify.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "dsl/stream/StateStream.hpp"
#include <cstring>

namespace derived_node_test
{
  using namespace loka::core;
  using namespace loka::app::scene;

  enum Declaration { PAIR, SINGLE, EARLY, COMPLETED_BATCH, OPEN_BATCH,
                     REFUSED_DEPENDENCY, REFUSED_DERIVED, NULL_EVAL };
  static Declaration declaration = PAIR;
  static int evaluations = 0;
  static int destructions = 0;
  static int allocations = 0;
  static int frees = 0;
  static int refusals = 0;
  static MutableState<bool> *visible = 0;

  void *allocate(std::size_t size, const LokaAllocationSite &site)
  {
    if (std::strcmp(site.ownerTag, "StateOwner") == 0 &&
        ((declaration == REFUSED_DEPENDENCY && size == sizeof(MutableState<int>)) ||
         (declaration == REFUSED_DERIVED && size == sizeof(DerivedState<String>))))
    {
      ++refusals;
      return 0;
    }
    void *storage = new (std::nothrow) char[size];
    if (storage)
      ++allocations;
    return storage;
  }
  void release(void *storage, const LokaAllocationSite &)
  {
    ++frees;
    delete[] static_cast<char *>(storage);
  }
  struct Probe
  {
    explicit Probe(Declaration mode)
    {
      declaration = mode;
      evaluations = destructions = allocations = frees = refusals = 0;
      LokaAllocSetBackend(&allocate, &release);
    }
    ~Probe()
    {
      LOKA_VERIFY(allocations == frees);
      LokaAllocSetBackend(0, 0);
    }
  };

  struct Sum : DerivedState<String>::EvalFn
  {
    Sum(const NodeState<int> &a, const NodeState<int> &b) : a_(a), b_(b) {}
    ~Sum() { ++destructions; }
    String operator()()
    {
      ++evaluations;
      // Quantized formatting lets a changed input produce an equal output.
      const int sum = this->a_.get() + (this->b_.isValid() ? this->b_.get() : 0);
      return sum < 10 ? String::Literal("small") : String::Literal("large");
    }
    const NodeState<int> &a_;
    const NodeState<int> &b_;
  };

  class Fixture : public BoundaryNodeFor<Fixture>
  {
  public:
    explicit Fixture(const BoundaryPropsFor<Fixture> &props)
        : BoundaryNodeFor<Fixture>(props)
    {
      if (declaration == EARLY)
        this->derived(this->sum_, this->a_, new Sum(this->a_, this->b_));
      if (declaration == COMPLETED_BATCH)
        this->declareStates(1).state(this->a_, 0);
      else if (declaration == OPEN_BATCH)
      {
        NodeStateBatch batch = this->declareStates(1);
        batch.state(this->a_, 0);
        this->derived(this->sum_, this->a_, new Sum(this->a_, this->b_));
        return;
      }
      else
        this->state(this->a_, 0);
      if (declaration == EARLY)
        return;
      if (declaration == NULL_EVAL)
        this->derived(this->sum_, this->a_, static_cast<DerivedState<String>::EvalFn *>(0));
      else if (declaration == PAIR)
      {
        this->state(this->b_, 0);
        this->derived(this->sum_, this->a_, this->b_, new Sum(this->a_, this->b_));
      }
      else
        this->derived(this->sum_, this->a_, new Sum(this->a_, this->b_));
    }
    virtual void *allocateStateMemory(size_t size, size_t align)
    {
      if (declaration == REFUSED_DEPENDENCY)
        return 0; // Force the mutable dependency through the existing heap gate.
      return BoundaryNodeFor<Fixture>::allocateStateMemory(size, align);
    }
    virtual void composeNode(NodeComposition &c) { c.declare(loka::app::Fragment()); }
    void write(int a, int b)
    {
      StateTrackerGuard guard(this->tracker());
      this->a_.set(a);
      if (this->b_.isValid())
        this->b_.set(b);
    }
    void writeA(int value)
    {
      StateTrackerGuard guard(this->tracker());
      this->a_.set(value);
    }
    void writeB(int value)
    {
      StateTrackerGuard guard(this->tracker());
      this->b_.set(value);
    }
    NodeState<int> a_, b_;
    DerivedNodeState<String> sum_;
  };

  // A bounded registration fixture lets release be checked while the owner
  // remains alive; owner teardown alone could hide a missing disconnect.
  class ChildSeats : public ComposableNode
  {
  public:
    ChildSeats()
    {
      this->state(this->a_, 0);
      this->derived(this->sum_, this->a_, new Sum(this->a_, this->b_));
    }
    virtual void composeWithContext(ComponentContext &, ComposeEvent) {}
    NodeState<int> a_, b_;
    DerivedNodeState<String> sum_;
  };

  struct Observer
  {
    explicit Observer(Fixture &node) : node_(node), calls_(0), value_() {}
    static void changed(void *data)
    {
      Observer &self = *static_cast<Observer *>(data);
      ++self.calls_;
      self.value_ = self.node_.sum_.get();
    }
    Fixture &node_;
    int calls_;
    String value_;
  };

  Fixture *root(Scene &scene)
  {
    return static_cast<Fixture *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  }

  class ParkHost : public BoundaryNodeFor<ParkHost>
  {
  public:
    explicit ParkHost(const BoundaryPropsFor<ParkHost> &props) : BoundaryNodeFor<ParkHost>(props) {}
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const { return false; }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(loka::app::Show(*visible) << Boundary<Fixture>());
    }
  };
  Fixture *findFixture(Node *node)
  {
    if (!node)
      return 0;
    BoundaryNode *boundary = node->asBoundary();
    if (boundary && boundary->propsTypeId() == BoundaryPropsFor<Fixture>().propsTypeId())
      return static_cast<Fixture *>(boundary);
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
    {
      Fixture *found = findFixture(child);
      if (found)
        return found;
    }
    return 0;
  }
}

void testDerivedNodeStateTransactionsAndTeardown()
{
  using namespace derived_node_test;
  Probe probe(PAIR);
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<Fixture>()));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    Fixture *node = root(scene);
    LOKA_VERIFY(node && node->sum_.isValid() && evaluations == 1);
    Observer observer(*node);
    node->sum_.bind(&Observer::changed, &observer, false);
    node->writeA(10);
    LOKA_VERIFY(evaluations == 2 && observer.calls_ == 1);
    LOKA_VERIFY(stateValuesEqual(observer.value_, String::Literal("large")));
    node->write(1, 2);
    LOKA_VERIFY(evaluations == 3 && observer.calls_ == 2);
    LOKA_VERIFY(stateValuesEqual(observer.value_, String::Literal("small")));
    node->writeA(2);
    LOKA_VERIFY(evaluations == 4 && observer.calls_ == 2);
    // A write to only the second input discriminates its dependency edge.
    node->writeB(10);
    LOKA_VERIFY(evaluations == 5 && observer.calls_ == 3);
    LOKA_VERIFY(stateValuesEqual(observer.value_, String::Literal("large")));
    node->sum_.unbind(&Observer::changed, &observer);
    LOKA_VERIFY(destructions == 0);
  }
  LOKA_VERIFY(destructions == 1 && allocations == frees);
  {
    Fixture owner((BoundaryPropsFor<Fixture>()));
    ComponentContext context;
    context.setStateOwner(&owner);
    {
      ChildSeats child;
      child.compose(context, COMPOSE_EVENT_ATTACH);
      LOKA_VERIFY(child.sum_.isValid());
    }
    LOKA_VERIFY(destructions == 2);
  }
  // The uncomposed owner's queued evaluator is also destroyed once.
  LOKA_VERIFY(destructions == 3 && allocations == frees);
}

void testDerivedNodeStateRefusalsAndBatchOrder()
{
  using namespace derived_node_test;
  const Declaration modes[] = { SINGLE, EARLY, COMPLETED_BATCH, OPEN_BATCH,
                                REFUSED_DEPENDENCY, REFUSED_DERIVED, NULL_EVAL };
  for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
  {
    Probe probe(modes[i]);
    {
      NullScenePlatformController platform;
      Scene scene((Boundary<Fixture>()));
      scene.mount(&platform);
      loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
      Fixture *node = root(scene);
      LOKA_VERIFY(node);
      const bool valid = modes[i] == SINGLE || modes[i] == COMPLETED_BATCH;
      LOKA_VERIFY(node->sum_.isValid() == valid);
      LOKA_VERIFY(evaluations == (valid ? 1 : 0));
      LOKA_VERIFY(node->composeResult().allocationFailed == !valid);
      LOKA_VERIFY(destructions == 0);
      if (valid)
      {
        node->writeA(10);
        LOKA_VERIFY(evaluations == 2);
        LOKA_VERIFY(stateValuesEqual(node->sum_.get(), String::Literal("large")));
      }
      if (modes[i] == REFUSED_DEPENDENCY)
        LOKA_VERIFY(!node->a_.isValid() && refusals > 0);
      if (modes[i] == REFUSED_DERIVED)
        LOKA_VERIFY(node->a_.isValid() && refusals > 0);
    }
    LOKA_VERIFY(destructions == (modes[i] == NULL_EVAL ? 0 : 1));
    LOKA_VERIFY(allocations == frees);
  }
}

void testDerivedNodeStateRetainedReattach()
{
  using namespace derived_node_test;
  Probe probe(PAIR);
  {
    MutableState<bool> condition(true);
    visible = &condition;
    NullScenePlatformController platform;
    Scene scene((Boundary<ParkHost>()));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    Fixture *node = findFixture(loka::dsl::testing::SceneTestAccess::rootNode(scene));
    LOKA_VERIFY(node && node->sum_.isValid());
    State<String> *identity = node->sum_.state();
    const int initialEvaluations = evaluations;
    condition.set(false);
    LOKA_VERIFY(scene.flushInvalidation());
    LOKA_VERIFY(node->lifecycleFact() == NODE_FACT_DETACHED_RETAINED);
    LOKA_VERIFY(destructions == 0);
    condition.set(true);
    LOKA_VERIFY(scene.flushInvalidation());
    LOKA_VERIFY(findFixture(loka::dsl::testing::SceneTestAccess::rootNode(scene)) == node);
    LOKA_VERIFY(node->lifecycleFact() == NODE_FACT_ATTACHED);
    LOKA_VERIFY(!node->composeResult().allocationFailed);
    LOKA_VERIFY(node->sum_.state() == identity && evaluations == initialEvaluations);
    LOKA_VERIFY(destructions == 0);
    node->writeA(10);
    LOKA_VERIFY(evaluations == initialEvaluations + 1);
    visible = 0;
  }
  LOKA_VERIFY(destructions == 1 && allocations == frees);
}
