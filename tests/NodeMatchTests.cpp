#include "NodeMatchTests.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include <cstdio>
#include "app/nodes/nestable/Keyed.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "testing/scene/OwnershipDump.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Match.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "support/RecordingPlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  struct MatchArmRecord;
  class MatchArmBoundaryNode;
  struct MatchArmTypeTag
  {
  };

  struct MatchArmProps : public loka::app::scene::NodePropsBase<MatchArmProps>
  {
    typedef MatchArmTypeTag TypeTag;
    typedef MatchArmBoundaryNode NodeType;

    explicit MatchArmProps(MatchArmRecord *recordValue = 0)
        : record(recordValue)
    {
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const MatchArmProps &other = static_cast<const MatchArmProps &>(rhs);
      return this->record < other.record;
    }

    MatchArmRecord *record;
  };

  struct MatchArmRecord
  {
    MatchArmRecord()
        : node(0),
          constructions(0),
          destructions(0),
          nextInstanceId(0)
    {
    }

    MatchArmBoundaryNode *node;
    int constructions;
    int destructions;
    int nextInstanceId;
  };

  class MatchArmBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<MatchArmProps>
  {
  public:
    explicit MatchArmBoundaryNode(const MatchArmProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MatchArmProps>(props),
          value_(),
          record_(props.record),
          instanceId_(0)
    {
      this->state(this->value_, 0);
      if (this->record_)
      {
        ++this->record_->constructions;
        this->instanceId_ = ++this->record_->nextInstanceId;
        this->record_->node = this;
      }
    }
    virtual ~MatchArmBoundaryNode()
    {
      if (this->record_)
      {
        ++this->record_->destructions;
        if (this->record_->node == this)
        {
          this->record_->node = 0;
        }
      }
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::FragmentDefinition());
    }
    int value() const
    {
      return this->value_.get();
    }
    void setValue(int value)
    {
      this->value_.set(value);
    }
    int instanceId() const
    {
      return this->instanceId_;
    }

  private:
    loka::app::scene::NodeState<int> value_;
    MatchArmRecord *record_;
    int instanceId_;
  };

  enum MatchScenario
  {
    MATCH_SCENARIO_DUPLICATE_VALUE,
    MATCH_SCENARIO_PREDICATE_ORDER,
    MATCH_SCENARIO_EMPTY,
    MATCH_SCENARIO_THREE_STATEFUL,
    MATCH_SCENARIO_POLICY
  };

  struct MatchInputs
  {
    MatchInputs(loka::core::MutableState<int> *selectionValue,
                MatchScenario scenarioValue)
        : selection(selectionValue),
          scenario(scenarioValue),
          predicateCalls(0),
          predicateLastValue(0)
    {
    }

    loka::core::MutableState<int> *selection;
    MatchScenario scenario;
    MatchArmRecord records[3];
    int predicateCalls;
    int predicateLastValue;
  };

  MatchInputs *g_matchInputs = 0;

  bool matchEvenPredicate(const int &value, void *userData)
  {
    MatchInputs *inputs = static_cast<MatchInputs *>(userData);
    if (!inputs)
    {
      return false;
    }
    ++inputs->predicateCalls;
    inputs->predicateLastValue = value;
    return value % 2 == 0;
  }

  class MatchRootBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<MatchRootBoundaryNode>
      MatchRootBoundaryProps;

  class MatchRootBoundaryNode : public loka::app::scene::BoundaryNodeFor<MatchRootBoundaryNode>
  {
  public:
    explicit MatchRootBoundaryNode(const MatchRootBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<MatchRootBoundaryNode>(props)
    {
    }
    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (!g_matchInputs || !g_matchInputs->selection)
      {
        composition.declare(root);
        return;
      }

      loka::app::scene::BoundaryDefinition<MatchArmProps, MatchArmBoundaryNode> arm0 =
          loka::app::scene::Boundary<MatchArmBoundaryNode>(
              MatchArmProps(&g_matchInputs->records[0]));
      loka::app::scene::BoundaryDefinition<MatchArmProps, MatchArmBoundaryNode> arm1 =
          loka::app::scene::Boundary<MatchArmBoundaryNode>(
              MatchArmProps(&g_matchInputs->records[1]));
      loka::app::scene::BoundaryDefinition<MatchArmProps, MatchArmBoundaryNode> arm2 =
          loka::app::scene::Boundary<MatchArmBoundaryNode>(
              MatchArmProps(&g_matchInputs->records[2]));
      loka::app::MatchDefinition<int> match =
          loka::app::Match(*g_matchInputs->selection);

      switch (g_matchInputs->scenario)
      {
      case MATCH_SCENARIO_DUPLICATE_VALUE:
        match.arm(0, arm0).arm(1, arm1).arm(0, arm2);
        break;
      case MATCH_SCENARIO_PREDICATE_ORDER:
        match.arm(&matchEvenPredicate, g_matchInputs, arm0)
            .arm(2, arm1)
            .otherwise(arm2);
        break;
      case MATCH_SCENARIO_EMPTY:
        match.arm(1, arm0);
        break;
      case MATCH_SCENARIO_THREE_STATEFUL:
        match.arm(0, arm0).arm(1, arm1).arm(2, arm2);
        break;
      case MATCH_SCENARIO_POLICY:
      {
        loka::app::PolicyScopeDefinition destroyScope;
        destroyScope.destroyOnDetach() << arm1;
        match.arm(0, arm0).arm(1, destroyScope);
        break;
      }
      }

      root << match;
      composition.declare(root);
    }
  };

  template <typename T>
  void setMatchState(loka::core::MutableState<T> &state, const T &value)
  {
    loka::core::StateTrackerGuard guard(state.trackerOwner());
    state.set(value);
  }

  void flushMatchState(loka::app::scene::Scene &scene)
  {
    assert(scene.hasPendingInvalidation()); // loka-assert-ok: pure query
    LOKA_VERIFY(scene.flushInvalidation());
  }

  loka::app::scene::BoundaryNode *rootBoundary(loka::app::scene::Scene &scene)
  {
    return loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  }
} // namespace

void testNodeMatchSelectsFirstDeclaredValueArm()
{
  loka::core::MutableState<int> selection(0);
  MatchInputs inputs(&selection, MATCH_SCENARIO_DUPLICATE_VALUE);
  g_matchInputs = &inputs;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<MatchRootBoundaryNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    LOKA_VERIFY(inputs.records[0].node);
    assert(inputs.records[0].constructions == 1);
    assert(inputs.records[1].constructions == 0);
    assert(inputs.records[2].constructions == 0);

    setMatchState(selection, 1);
    flushMatchState(scene);
    LOKA_VERIFY(inputs.records[1].node);
    assert(inputs.records[2].constructions == 0 &&
           "the later duplicate arm never materializes");
  }
  g_matchInputs = 0;
}

void testNodeMatchPredicatePrecedesValueArmAndRunsOncePerVisit()
{
  loka::core::MutableState<int> selection(2);
  MatchInputs inputs(&selection, MATCH_SCENARIO_PREDICATE_ORDER);
  g_matchInputs = &inputs;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<MatchRootBoundaryNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    LOKA_VERIFY(inputs.records[0].node);
    assert(inputs.records[1].constructions == 0 &&
           "the earlier predicate wins over a matching value arm");
    assert(inputs.predicateCalls == 1 && inputs.predicateLastValue == 2);

    setMatchState(selection, 3);
    flushMatchState(scene);
    LOKA_VERIFY(inputs.records[2].node);
    assert(inputs.predicateCalls == 2 && inputs.predicateLastValue == 3 &&
           "selection invokes the predicate once per Boundary visit");
  }
  g_matchInputs = 0;
}

void testNodeMatchEmptySeatRematerializes()
{
  loka::core::MutableState<int> emptyRootSelection(1);
  loka::app::MatchDefinition<int> emptyRoot =
      loka::app::Match(emptyRootSelection);
  emptyRoot.arm(1, static_cast<loka::app::scene::NodeDefinitionBase *>(0));
  loka::app::scene::NodeDefinitionBase *normalizedRoot =
      emptyRoot.armDefinition(0);
  LOKA_VERIFY(normalizedRoot);
  loka::app::scene::INestableDefinition *normalizedNestable =
      normalizedRoot->asNestableDefinition();
  LOKA_VERIFY(normalizedNestable);
  const size_t normalizedChildCount = normalizedNestable->childrenCount();
  LOKA_VERIFY(normalizedChildCount == 0 &&
              "a null owned arm definition normalizes to an empty Fragment");

  loka::core::MutableState<int> selection(99);
  MatchInputs inputs(&selection, MATCH_SCENARIO_EMPTY);
  g_matchInputs = &inputs;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<MatchRootBoundaryNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(inputs.records[0].constructions == 0);

    setMatchState(selection, 1);
    flushMatchState(scene);
    LOKA_VERIFY(inputs.records[0].node);
    assert(inputs.records[0].constructions == 1);
  }
  g_matchInputs = 0;
}

void testNodeMatchOtherwiseIsTheLastArm()
{
  loka::core::MutableState<int> unmatched(99);
  loka::app::FragmentDefinition arm0;
  loka::app::FragmentDefinition arm1;
  loka::app::FragmentDefinition arm2;
  loka::app::FragmentDefinition fallback;
  loka::app::MatchDefinition<int> match = loka::app::Match(unmatched);
  match.arm(10, arm0).arm(20, arm1).arm(30, arm2).otherwise(fallback);

  const unsigned armCount = match.armCount();
  LOKA_VERIFY(armCount == 4);
  loka::app::scene::NodeDefinitionBase *otherwiseRoot = match.armDefinition(3);
  LOKA_VERIFY(otherwiseRoot);
  assert(match.retainedDefinitionBranch(3) == otherwiseRoot);
  unsigned selectedArm = 0;
  const bool selected = match.selectArm(selectedArm);
  LOKA_VERIFY(selected);
  assert(selectedArm == 3);

  loka::core::MutableState<long> otherTypeState(99);
  loka::app::MatchDefinition<long> otherType = loka::app::Match(otherTypeState);
  assert(match.branchSeatTypeId() != otherType.branchSeatTypeId() &&
         "Match seat identity is distinct for each observed value type");

  loka::core::MutableState<int> matched(20);
  loka::app::MatchDefinition<int> earlier = loka::app::Match(matched);
  earlier.arm(10, arm0).arm(20, arm1).arm(30, arm2).otherwise(fallback);
  selectedArm = 0;
  const bool selectedEarlier = earlier.selectArm(selectedArm);
  LOKA_VERIFY(selectedEarlier);
  assert(selectedArm == 1);
}

void testNodeMatchRestoresThreeIndependentArmStates()
{
  loka::core::MutableState<int> selection(0);
  MatchInputs inputs(&selection, MATCH_SCENARIO_THREE_STATEFUL);
  g_matchInputs = &inputs;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<MatchRootBoundaryNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    loka::app::scene::BoundaryNode *root = rootBoundary(scene);
    LOKA_VERIFY(root && inputs.records[0].node);
    MatchArmBoundaryNode *arm0 = inputs.records[0].node;
    arm0->setValue(10);

    setMatchState(selection, 1);
    flushMatchState(scene);
    LOKA_VERIFY(inputs.records[1].node);
    inputs.records[1].node->setValue(20);
    const unsigned parkedAfterB = root->parkedBranchCountForTesting();
    LOKA_VERIFY(parkedAfterB == 1);

    setMatchState(selection, 2);
    flushMatchState(scene);
    LOKA_VERIFY(inputs.records[2].node);
    inputs.records[2].node->setValue(30);
    const unsigned parkedAfterC = root->parkedBranchCountForTesting();
    LOKA_VERIFY(parkedAfterC == 2);

    setMatchState(selection, 0);
    flushMatchState(scene);
    assert(inputs.records[0].node == arm0 && arm0->value() == 10);
    const unsigned parkedAfterAReentry = root->parkedBranchCountForTesting();
    LOKA_VERIFY(parkedAfterAReentry == 2);

    setMatchState(selection, 1);
    flushMatchState(scene);
    assert(inputs.records[1].node && inputs.records[1].node->value() == 20);
    setMatchState(selection, 2);
    flushMatchState(scene);
    assert(inputs.records[2].node && inputs.records[2].node->value() == 30);
  }
  g_matchInputs = 0;
}

void testNodeMatchDestroyOnDetachIsPerArm()
{
  loka::core::MutableState<int> selection(0);
  MatchInputs inputs(&selection, MATCH_SCENARIO_POLICY);
  g_matchInputs = &inputs;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<MatchRootBoundaryNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    loka::app::scene::BoundaryNode *root = rootBoundary(scene);
    LOKA_VERIFY(root && inputs.records[0].node);
    MatchArmBoundaryNode *arm0 = inputs.records[0].node;

    setMatchState(selection, 1);
    flushMatchState(scene);
    LOKA_VERIFY(inputs.records[1].node);
    const unsigned parkedAfterEnteringB = root->parkedBranchCountForTesting();
    const unsigned parkedArmAfterEnteringB =
        root->parkedBranchArmForTesting(0);
    LOKA_VERIFY(parkedAfterEnteringB == 1 &&
                parkedArmAfterEnteringB == 0);
    const int firstBInstance = inputs.records[1].node->instanceId();

    setMatchState(selection, 0);
    flushMatchState(scene);
    const unsigned parkedAfterLeavingB = root->parkedBranchCountForTesting();
    LOKA_VERIFY(inputs.records[0].node == arm0 &&
                parkedAfterLeavingB == 0 &&
                "leaving the scoped arm destroys it instead of parking it");

    setMatchState(selection, 1);
    flushMatchState(scene);
    LOKA_VERIFY(inputs.records[1].node);
    const int secondBInstance = inputs.records[1].node->instanceId();
    LOKA_VERIFY(secondBInstance != firstBInstance &&
                inputs.records[1].constructions == 2 &&
                "the destroy-on-detach arm materializes a fresh subtree");
    const unsigned parkedAfterReturningB = root->parkedBranchCountForTesting();
    const unsigned parkedArmAfterReturningB =
        root->parkedBranchArmForTesting(0);
    LOKA_VERIFY(parkedAfterReturningB == 1 &&
                parkedArmAfterReturningB == 0 &&
                "the unscoped arm still parks normally");
  }
  g_matchInputs = 0;
}

void testNodeMatchCapacityRefusesOverflow()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  LOKA_VERIFY(child >= 0);
  if (child == 0)
  {
    loka::core::MutableState<int> selection(0);
    loka::app::FragmentDefinition empty;
    loka::app::MatchDefinition<int> match = loka::app::Match(selection);
    for (int value = 0;
         value < loka::app::MatchDefinition<int>::MAX_ARMS + 1;
         ++value)
    {
      match.arm(value, empty);
    }
    _exit(0);
  }
  int status = 0;
  LOKA_VERIFY(waitpid(child, &status, 0) == child);
  LOKA_VERIFY(WIFSIGNALED(status));
  LOKA_VERIFY(WTERMSIG(status) == SIGABRT);
#elif defined(NDEBUG)
  loka::core::MutableState<int> selection(0);
  loka::app::FragmentDefinition empty;
  loka::app::MatchDefinition<int> match = loka::app::Match(selection);
  for (int value = 0;
       value < loka::app::MatchDefinition<int>::MAX_ARMS + 1;
       ++value)
  {
    match.arm(value, empty);
  }
  const unsigned armCount = match.armCount();
  LOKA_VERIFY(armCount == loka::app::MatchDefinition<int>::MAX_ARMS);
#endif
}

namespace
{
  using namespace loka::app::scene;
  struct KeyedProbeNode;
  struct KeyedProbeTag
  {
  };
  struct KeyedProbeRecord
  {
    KeyedProbeRecord()
        : owner(0),
          declarations(0),
          compositions(0),
          bindings(0),
          destroyed(0),
          value(7),
          refuse(false),
          nested(false),
          section(false),
          nestedFailure(false),
          directSeat(false),
          sharedSource(false),
          declarationOwner(0),
          leaf(0),
          switchState(false),
          otherState(false)
    {
    }
    KeyedProbeNode *owner;
    int declarations, compositions, bindings, destroyed, value;
    bool refuse, nested, section, nestedFailure, directSeat, sharedSource;
    IStateOwner *declarationOwner;
    Node *leaf;
    loka::core::MutableState<bool> switchState;
    loka::core::MutableState<bool> otherState;
  };
  struct KeyedLeafTag
  {
  };
  class KeyedLeaf;
  struct KeyedLeafProps : NodePropsBase<KeyedLeafProps>
  {
    typedef KeyedLeafTag TypeTag;
    typedef KeyedLeaf NodeType;
    KeyedLeafProps(KeyedProbeRecord *r = 0, int v = 0)
        : record(r),
          value(v)
    {
    }
    bool operator<(const PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
        return false;
      const KeyedLeafProps &other = static_cast<const KeyedLeafProps &>(rhs);
      return this->value < other.value;
    }
    KeyedProbeRecord *record;
    int value;
  };
  class KeyedLeaf : public loka::app::FragmentNode
  {
  public:
    typedef KeyedLeafTag TypeTag;
    explicit KeyedLeaf(const KeyedLeafProps &p)
        : loka::app::FragmentNode(loka::app::FragmentProps()),
          record_(p.record),
          value(p.value),
          props(p)
    {
      this->record_->leaf = this;
    }
    virtual ~KeyedLeaf()
    {
      ++this->record_->destroyed;
    }
    KeyedProbeRecord *record_;
    int value;
    KeyedLeafProps props;
  };
  class KeyedLeafDefinition : public NodeDefinition<KeyedLeafProps, KeyedLeaf>
  {
  public:
    explicit KeyedLeafDefinition(KeyedProbeRecord &r)
        : NodeDefinition<KeyedLeafProps, KeyedLeaf>(KeyedLeafProps(&r, r.value))
    {
    }
    virtual NodeDefinitionBase *clone() const
    {
      return new KeyedLeafDefinition(*this);
    }
    virtual Node *create() const
    {
      return this->props.record->refuse ? 0 : NodeDefinition<KeyedLeafProps, KeyedLeaf>::create();
    }
    virtual Node *createInPlace(void *p) const
    {
      return this->props.record->refuse ? 0 : NodeDefinition<KeyedLeafProps, KeyedLeaf>::createInPlace(p);
    }
  };
  struct KeyedProbeProps : NodePropsBase<KeyedProbeProps>
  {
    typedef KeyedProbeTag TypeTag;
    typedef KeyedProbeNode NodeType;
    explicit KeyedProbeProps(KeyedProbeRecord *r = 0)
        : record(r)
    {
    }
    bool operator<(const PropsBase &) const
    {
      return false;
    }
    KeyedProbeRecord *record;
  };
  struct KeyedProbeNode : StdCompositionBoundaryNodeBase<KeyedProbeProps>
  {
    explicit KeyedProbeNode(const KeyedProbeProps &p)
        : StdCompositionBoundaryNodeBase<KeyedProbeProps>(p),
          bank_()
    {
      this->state(this->bank_, 0);
      this->props.record->owner = this;
    }
    void changeKey(int value, bool force = false)
    {
      this->bank_.set(value, force);
    }
    virtual void declareBindings(BindingToken &)
    {
      ++this->props.record->bindings;
    }
    virtual void composeNode(NodeComposition &c)
    {
      ++this->props.record->compositions;
      if (this->props.record->section)
        c.declare(loka::app::BoundarySection(810)
                  << loka::app::Keyed(*this->bank_.state(), this, &KeyedProbeNode::declareBranch));
      else if (this->props.record->sharedSource)
        c.declare(loka::app::Fragment()
                  << loka::app::Keyed(*this->bank_.state(), this, &KeyedProbeNode::declareBranch)
                  << loka::app::Match(this->props.record->switchState).arm(false, loka::app::Fragment()));
      else
        c.declare(
            loka::app::Fragment() << loka::app::Keyed(*this->bank_.state(), this, &KeyedProbeNode::declareBranch));
    }
    void declareBranch(NodeComposition &c)
    {
      KeyedProbeRecord &r = *this->props.record;
      ++r.declarations;
      r.declarationOwner = c.componentContext()->stateOwner();
      if (r.directSeat)
      {
        loka::app::PolicyScopeDefinition first;
        first.destroyOnDetach() << KeyedLeafDefinition(r);
        c.declare(loka::app::Match(r.switchState).arm(false, first).arm(true, KeyedLeafDefinition(r)));
      }
      else if (r.nestedFailure)
        c.declare(loka::app::Fragment() << loka::app::Keyed(r.switchState, this, &KeyedProbeNode::declareNested)
                                        << KeyedLeafDefinition(r));
      else if (r.nested)
        c.declare(loka::app::Fragment() << loka::app::Match(r.switchState)
                                               .arm(false, KeyedLeafDefinition(r))
                                               .arm(true,
                                                    loka::app::Fragment() << loka::app::Match(r.switchState)
                                                                                 .arm(true, KeyedLeafDefinition(r))));
      else
        c.declare(KeyedLeafDefinition(r));
    }
    void declareNested(NodeComposition &c)
    {
      c.declare(loka::app::Match(this->props.record->otherState)
                    .arm(false, KeyedLeafDefinition(*this->props.record))
                    .arm(true, KeyedLeafDefinition(*this->props.record)));
    }
    NodeState<int> bank_;
  };
} // namespace

void testKeyedRedeclaresCurrentMembersOnceAndReclaimsOnDrain()
{
  KeyedProbeRecord r;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<KeyedProbeNode>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);
  LOKA_VERIFY(r.declarations == 1 && r.compositions == 1 && r.bindings == 1);
  loka::app::scene::Node *old = r.leaf;
  loka::app::scene::Node *oldRoot = r.owner->childrenHead()->asNestable()->childrenHead();
  const int destroyed = r.destroyed;
  r.value = 19;
  r.owner->changeKey(1);
  LOKA_VERIFY(r.declarations == 2 && r.compositions == 1 && r.bindings == 1);
  LOKA_VERIFY(r.leaf != old && static_cast<KeyedLeaf *>(r.leaf)->value == 19);
  const loka::app::scene::NodeLifecycleFact oldFact = old->lifecycleFact();
  const loka::app::scene::NodeLifecycleFact oldRootFact = oldRoot->lifecycleFact();
  const loka::app::scene::Node *newRoot = r.owner->childrenHead()->asNestable()->childrenHead();
  LOKA_VERIFY(oldFact == loka::app::scene::NODE_FACT_RETIRED);
  LOKA_VERIFY(oldRootFact == loka::app::scene::NODE_FACT_RETIRED && newRoot != oldRoot);
  const bool pending = scene.hasPendingInvalidation();
  LOKA_VERIFY(r.destroyed == destroyed && pending);
  const bool applied = scene.flushInvalidation();
  const bool pendingAfterDrain = scene.hasPendingInvalidation();
  LOKA_VERIFY(!applied && r.destroyed == destroyed + 1 && !pendingAfterDrain);
  r.owner->changeKey(1);
  r.owner->changeKey(1, true);
  LOKA_VERIFY(r.declarations == 2 && r.compositions == 1 && r.bindings == 1);
  const bool pendingSameKey = scene.hasPendingInvalidation();
  LOKA_VERIFY(!pendingSameKey);
}

void testKeyedRefusedDeclarationKeepsLiveBranchAndRetriesCurrentKey()
{
  KeyedProbeRecord r;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<KeyedProbeNode>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);
  loka::app::scene::Node *old = r.leaf;
  const int destroyed = r.destroyed;
  r.refuse = true;
  r.owner->changeKey(1);
  const bool allocationFailed = r.owner->composeResult().allocationFailed;
  LOKA_VERIFY(allocationFailed);
  const loka::app::scene::NodeLifecycleFact oldFact = old->lifecycleFact();
  LOKA_VERIFY(r.leaf == old && oldFact == loka::app::scene::NODE_FACT_ATTACHED);
  LOKA_VERIFY(r.destroyed == destroyed && r.declarations == 2 && r.compositions == 1);
  r.refuse = false;
  r.value = 23;
  r.owner->changeKey(1, true);
  LOKA_VERIFY(r.leaf != old && static_cast<KeyedLeaf *>(r.leaf)->value == 23);
  LOKA_VERIFY(r.declarations == 3 && r.compositions == 1);
  const bool applied = scene.flushInvalidation();
  LOKA_VERIFY(!applied && r.destroyed == destroyed + 1);
}

void testKeyedDeclarationUsesEnclosingSectionOnMountAndUpdate()
{
  KeyedProbeRecord r;
  r.section = true;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<KeyedProbeNode>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);
  loka::app::scene::IStateOwner *section = r.declarationOwner;
  LOKA_VERIFY(section && section != r.owner);
  r.owner->changeKey(1);
  LOKA_VERIFY(r.declarationOwner == section && r.declarations == 2);
  const bool applied = scene.flushInvalidation();
  LOKA_VERIFY(!applied);
}

void testKeyedOwnsFreshNestedSeatPlansAcrossReplacement()
{
  KeyedProbeRecord r;
  r.nested = true;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<KeyedProbeNode>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);
  {
    loka::core::StateTrackerGuard guard(r.switchState.trackerOwner());
    r.switchState.set(true);
  }
  r.value = 31;
  r.owner->changeKey(1);
  LOKA_VERIFY(r.declarations == 2 && static_cast<KeyedLeaf *>(r.leaf)->value == 31);
  const bool applied = scene.flushInvalidation();
  LOKA_VERIFY(!applied);
  {
    loka::core::StateTrackerGuard guard(r.switchState.trackerOwner());
    r.switchState.set(false);
  }
  LOKA_VERIFY(r.declarations == 2 && r.compositions == 1);
  {
    loka::core::StateTrackerGuard guard(r.switchState.trackerOwner());
    r.switchState.set(true);
  }
  LOKA_VERIFY(r.declarations == 2 && r.compositions == 1);
}

void testKeyedFailedOuterCandidatePublishesNoNestedObservations()
{
  KeyedProbeRecord r;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<KeyedProbeNode>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);
  const std::string before = loka::dsl::testing::OwnershipDump::dump(scene);
  r.nestedFailure = true;
  r.refuse = true;
  r.owner->changeKey(1);
  const bool allocationFailed = r.owner->composeResult().allocationFailed;
  LOKA_VERIFY(allocationFailed);
  const std::string after = loka::dsl::testing::OwnershipDump::dump(scene);
  LOKA_VERIFY(after == before);
}

void testKeyedDirectSeatRootSurvivesInnerSwitchAndDrain()
{
  KeyedProbeRecord r;
  r.directSeat = true;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<KeyedProbeNode>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);
  {
    loka::core::StateTrackerGuard guard(r.switchState.trackerOwner());
    r.switchState.set(true);
  }
  const bool applied = scene.flushInvalidation();
  LOKA_VERIFY(!applied);
  r.value = 43;
  r.owner->changeKey(1);
  const bool pending = scene.hasPendingInvalidation();
  LOKA_VERIFY(pending && r.declarations == 2);
  const bool drained = scene.flushInvalidation();
  LOKA_VERIFY(!drained);
}

void testKeyedRemovesOnlyUnsharedOutgoingObservations()
{
  for (int shared = 0; shared != 2; ++shared)
  {
    KeyedProbeRecord r;
    r.nested = true;
    r.sharedSource = shared != 0;
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<KeyedProbeNode>(KeyedProbeProps(&r))));
    scene.mount(&platform);
    scene.updateAttached(true);
    r.nested = false;
    r.owner->changeKey(1);
    const bool applied = scene.flushInvalidation();
    LOKA_VERIFY(!applied);
    const std::string dump = loka::dsl::testing::OwnershipDump::dump(scene);
    const std::string expected = shared ? "observed: 2" : "observed: 1";
    const bool countsMatch = dump.find(expected) != std::string::npos;
    LOKA_VERIFY(countsMatch);
  }
}

void testKeyedRetiresNestedParkedScopeBeforeDeclarationReset()
{
  KeyedProbeRecord r;
  r.nestedFailure = true;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<KeyedProbeNode>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);

  {
    loka::core::StateTrackerGuard guard(r.otherState.trackerOwner());
    r.otherState.set(true);
  }
  const std::string before = loka::dsl::testing::OwnershipDump::dump(scene);
  const bool hasParked = before.find("parked\n") != std::string::npos;
  const unsigned parkedBefore = r.owner->parkedBranchCountForTesting();
  LOKA_VERIFY(hasParked && parkedBefore == 1 && r.destroyed == 0);
  r.nestedFailure = false;
  r.owner->changeKey(1);
  const std::string after = loka::dsl::testing::OwnershipDump::dump(scene);
  const bool hasParkedAfter = after.find("parked\n") != std::string::npos;
  const bool pending = scene.hasPendingInvalidation();
  const unsigned parkedAfter = r.owner->parkedBranchCountForTesting();
  LOKA_VERIFY(!hasParkedAfter && parkedAfter == 0);
  LOKA_VERIFY(pending && r.destroyed == 0);
  const bool applied = scene.flushInvalidation();
  LOKA_VERIFY(!applied && r.destroyed == 3);
  // Force another boundary-wide parked sweep after the old scope was freed.
  r.owner->changeKey(1, true);
  LOKA_VERIFY(r.declarations == 2);
}

namespace
{
  struct ComposeSeatWriteTag
  {
  };
  struct ComposeSeatWriteNode;
  struct ComposeSeatWriteProps : NodePropsBase<ComposeSeatWriteProps>
  {
    typedef ComposeSeatWriteTag TypeTag;
    typedef ComposeSeatWriteNode NodeType;
    explicit ComposeSeatWriteProps(KeyedProbeRecord *r = 0)
        : record(r)
    {
    }
    bool operator<(const PropsBase &) const
    {
      return false;
    }
    KeyedProbeRecord *record;
  };
  struct ComposeSeatWriteNode : ComposableNode
  {
    typedef ComposeSeatWriteTag TypeTag;
    explicit ComposeSeatWriteNode(const ComposeSeatWriteProps &p)
        : props(p)
    {
    }
    virtual void composeWithContext(ComponentContext &, ComposeEvent event)
    {
      if (event == COMPOSE_EVENT_UPDATE && this->props.record->owner->bank_.get() == 0)
        this->props.record->owner->changeKey(1);
    }
    virtual void declareDirtySources(DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(&this->props.record->switchState, NODE_DIRTY_PROPS);
    }
    ComposeSeatWriteProps props;
  };
  struct ComposeSeatWriteBoundary : KeyedProbeNode
  {
    explicit ComposeSeatWriteBoundary(const KeyedProbeProps &p)
        : KeyedProbeNode(p)
    {
    }
    virtual void composeNode(NodeComposition &c)
    {
      ++this->props.record->compositions;
      c.declare(loka::app::Fragment() << loka::app::Keyed(
                    *this->bank_.state(), this, &ComposeSeatWriteBoundary::declareBranch)
                                      << NodeDefinition<ComposeSeatWriteProps, ComposeSeatWriteNode>(
                                             ComposeSeatWriteProps(this->props.record)));
    }
    void declareBranch(NodeComposition &c)
    {
      KeyedProbeNode::declareBranch(c);
    }
  };
} // namespace

void testKeyedComposeWriteRetainsSeatDirtyClassification()
{
  KeyedProbeRecord r;
  SceneTestSupport::RecordingPlatformController platform;
  Scene scene((Boundary<ComposeSeatWriteBoundary>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);
  platform.clearChanges();
  r.owner->markViewDirty(NODE_DIRTY_PROPS);
  const bool applied = scene.flushInvalidation();
  (void)applied;
  const size_t count = platform.changeCount();
  LOKA_VERIFY(count > 0);
  const NodeDirtyFlags flags = platform.changeAt(count - 1).flags;
  const int expected = NODE_DIRTY_CHILD | NODE_DIRTY_LAYOUT;
  LOKA_VERIFY(r.declarations == 2 && r.compositions == 1);
  LOKA_VERIFY(flags == expected);
}

void testKeyedOutgoingSeatSourceRemainsObservedByOrdinaryNode()
{
  KeyedProbeRecord r;
  r.nested = true;
  SceneTestSupport::RecordingPlatformController platform;
  Scene scene((Boundary<ComposeSeatWriteBoundary>(KeyedProbeProps(&r))));
  scene.mount(&platform);
  scene.updateAttached(true);
  r.nested = false;
  r.owner->changeKey(1);
  const bool drained = scene.flushInvalidation();
  (void)drained;
  platform.clearChanges();
  {
    loka::core::StateTrackerGuard transaction(r.switchState.trackerOwner());
    r.switchState.set(true);
  }
  const NodeDirtyFlags flags = platform.flagsSeenForNode(r.owner);
  LOKA_VERIFY(flags == NODE_DIRTY_PROPS && r.declarations == 2);
}

namespace
{
  /** Stack-owned observations for the plain-root traversal contract. */
  struct PlainRootRecord
  {
    PlainRootRecord()
        : selection(0), compositions(0), rootUpdates(0), childUpdates(0)
    {
    }
    loka::core::MutableState<int> selection;
    int compositions;
    int rootUpdates;
    int childUpdates;
    MatchArmRecord arms[2];
  };

  struct PlainRootNode;
  struct PlainRootChild;
  template <class NodeT> struct PlainRootTag {};
  template <class NodeT> struct PlainRootPropsFor : NodePropsBase<PlainRootPropsFor<NodeT> >
  {
    typedef PlainRootTag<NodeT> TypeTag;
    typedef NodeT NodeType;
    explicit PlainRootPropsFor(PlainRootRecord *value = 0, bool seatValue = false)
        : record(value), seat(seatValue) {}
    bool operator<(const PropsBase &) const { return false; }
    PlainRootRecord *record;
    bool seat;
  };

  typedef PlainRootPropsFor<PlainRootNode> PlainRootProps;
  typedef PlainRootPropsFor<PlainRootChild> PlainRootChildProps;

  struct PlainRootChild : ComposableNode
  {
    typedef PlainRootTag<PlainRootChild> TypeTag;
    explicit PlainRootChild(const PlainRootChildProps &p) : props(p) {}
    virtual void composeWithContext(ComponentContext &, ComposeEvent event)
    {
      if (event == COMPOSE_EVENT_UPDATE)
        ++this->props.record->childUpdates;
    }
    PlainRootChildProps props;
  };

  struct PlainRootNode : ComposableNode
  {
    typedef PlainRootTag<PlainRootNode> TypeTag;
    explicit PlainRootNode(const PlainRootProps &p) : props(p) {}
    PlainRootProps props;
    virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
    {
      if (event == COMPOSE_EVENT_UPDATE)
        ++this->props.record->rootUpdates;
      if (event != COMPOSE_EVENT_ATTACH || this->childrenHead())
        return;
      NodeComposition &composition = this->beginComposition(context);
      {
        NodeComposition::CompositionScope scope(composition);
        this->composeNode(composition);
      }
      context.boundary()->appendNestedBranchSeatPlan(composition);
      context.setComposition(&composition);
      this->addChild(composition.createNodeTree());
      context.setComposition(0);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      ++this->props.record->compositions;
      loka::app::Fragment root;
      root << NodeDefinition<PlainRootChildProps, PlainRootChild>(PlainRootChildProps(this->props.record));
      if (this->props.seat)
      {
        root << loka::app::Match(this->props.record->selection)
                    .arm(0, Boundary<MatchArmBoundaryNode>(MatchArmProps(&this->props.record->arms[0])))
                    .arm(1, Boundary<MatchArmBoundaryNode>(MatchArmProps(&this->props.record->arms[1])));
      }
      composition.declare(root);
    }
  };
}

void testPlainRootMatchUpdateComposesOnceAndWalksChildrenOnce()
{
  PlainRootRecord record;
  SceneTestSupport::RecordingPlatformController platform;
  Scene scene(new NodeDefinition<PlainRootProps, PlainRootNode>(PlainRootProps(&record, true)));
  scene.mount(&platform);
  scene.updateAttached(true);
  BoundaryNode *wrapper = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  LOKA_VERIFY(wrapper && wrapper->childrenHead() && !wrapper->childrenHead()->asBoundary());
  Node *plainRoot = wrapper->childrenHead();
  LOKA_VERIFY(record.compositions == 1 && record.arms[0].node && !record.arms[1].node);

  for (int selection = 1; selection >= 0; --selection)
  {
    const int updates = record.rootUpdates;
    const int childUpdates = record.childUpdates;
    setMatchState(record.selection, selection);
    // The ordinary wrapper may flush synchronously when the State commits.
    if (scene.hasPendingInvalidation())
      scene.flushInvalidation();
    LOKA_VERIFY(wrapper->childrenHead() == plainRoot && record.compositions == 1);
    LOKA_VERIFY(record.rootUpdates == updates + 1 && record.childUpdates == childUpdates + 1);
    LOKA_VERIFY(record.arms[selection].node &&
                record.arms[selection].node->lifecycleFact() == NODE_FACT_ATTACHED);
    LOKA_VERIFY(!record.arms[1 - selection].node ||
                record.arms[1 - selection].node->lifecycleFact() != NODE_FACT_ATTACHED);
    const PlatformApplyPlan &plan = loka::dsl::testing::SceneTestAccess::lastApplyPlan(scene);
    LOKA_VERIFY(plan.structureChanged);
  }
}

namespace
{
  template <int Depth> struct NestedWalkBoundary;
  template <int Depth>
  struct NestedWalkBoundary
      : StdCompositionBoundaryNodeBase<PlainRootPropsFor<NestedWalkBoundary<Depth> > >
  {
    typedef PlainRootPropsFor<NestedWalkBoundary<Depth> > Props;
    explicit NestedWalkBoundary(const Props &p)
        : StdCompositionBoundaryNodeBase<Props>(p) {}
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Boundary<NestedWalkBoundary<Depth - 1> >(
          PlainRootPropsFor<NestedWalkBoundary<Depth - 1> >(this->props.record)));
    }
  };

  template <> struct NestedWalkBoundary<0>
      : StdCompositionBoundaryNodeBase<PlainRootPropsFor<NestedWalkBoundary<0> > >
  {
    typedef PlainRootPropsFor<NestedWalkBoundary<0> > Props;
    explicit NestedWalkBoundary(const Props &p)
        : StdCompositionBoundaryNodeBase<Props>(p) {}
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(NodeDefinition<PlainRootChildProps, PlainRootChild>(
          PlainRootChildProps(this->props.record)));
    }
  };

  template <int Depth> void verifyNestedBoundaryUpdateWalk()
  {
    PlainRootRecord record;
    NullScenePlatformController platform;
    Scene scene(new BoundaryDefinition<PlainRootPropsFor<NestedWalkBoundary<Depth> >,
                                       NestedWalkBoundary<Depth> >(
        PlainRootPropsFor<NestedWalkBoundary<Depth> >(&record)));
    scene.mount(&platform);
    scene.updateAttached(true);
    const int before = record.childUpdates;
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
    const int visits = record.childUpdates - before;
    std::fprintf(stderr, "nested Std depth %d: leaf UPDATE visits = %d\n", Depth, visits);
    LOKA_VERIFY(visits == 1);

    BoundaryNode *root = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
    LOKA_VERIFY(root && root->childrenHead());
    BoundaryNode *nested = root->childrenHead()->asBoundary();
    LOKA_VERIFY(nested);
    nested->setFrozen(true);
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
    LOKA_VERIFY(record.childUpdates == before + 1);
    nested->setFrozen(false);
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
    LOKA_VERIFY(record.childUpdates == before + 2);
  }
}

void testNestedStdBoundaryUpdateWalksChildrenOnce()
{
  verifyNestedBoundaryUpdateWalk<1>();
}

void testDoublyNestedStdBoundaryUpdateWalksChildrenOnce()
{
  verifyNestedBoundaryUpdateWalk<2>();
}

namespace
{
  /** Per-generation observations distinguish the incoming leaf from its predecessor. */
  struct KeyedRootLeafEvents
  {
    KeyedRootLeafEvents() : constructions(0), attaches(0), updates(0) {}
    int constructions;
    int attaches;
    int updates;
  };

  struct KeyedRootWalkRecord
  {
    KeyedRootWalkRecord() : key(0) {}
    loka::core::MutableState<int> key;
    KeyedRootLeafEvents leaves[2];
  };

  template <class NodeT> struct KeyedRootWalkProps : NodePropsBase<KeyedRootWalkProps<NodeT> >
  {
    typedef PlainRootTag<NodeT> TypeTag;
    typedef NodeT NodeType;
    explicit KeyedRootWalkProps(KeyedRootWalkRecord *value) : record(value) {}
    bool operator<(const PropsBase &) const { return false; }
    KeyedRootWalkRecord *record;
  };

  struct KeyedRootWalkLeaf : ComposableNode
  {
    typedef PlainRootTag<KeyedRootWalkLeaf> TypeTag;
    explicit KeyedRootWalkLeaf(const KeyedRootWalkProps<KeyedRootWalkLeaf> &p)
        : props(p), events_(p.record->leaves[p.record->key.get()])
    {
      ++this->events_.constructions;
    }
    virtual void composeWithContext(ComponentContext &, ComposeEvent event)
    {
      if (event == COMPOSE_EVENT_ATTACH)
        ++this->events_.attaches;
      else if (event == COMPOSE_EVENT_UPDATE)
        ++this->events_.updates;
    }
    KeyedRootWalkProps<KeyedRootWalkLeaf> props;

  private:
    KeyedRootLeafEvents &events_;
  };

  struct KeyedRootWalkBoundary
      : StdCompositionBoundaryNodeBase<KeyedRootWalkProps<KeyedRootWalkBoundary> >
  {
    typedef KeyedRootWalkProps<KeyedRootWalkBoundary> Props;
    explicit KeyedRootWalkBoundary(const Props &p) : StdCompositionBoundaryNodeBase<Props>(p) {}
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(loka::app::Keyed(this->props.record->key, this,
                                          &KeyedRootWalkBoundary::declareLeaf));
    }
    void declareLeaf(NodeComposition &composition)
    {
      composition.declare(NodeDefinition<KeyedRootWalkProps<KeyedRootWalkLeaf>, KeyedRootWalkLeaf>(
          KeyedRootWalkProps<KeyedRootWalkLeaf>(this->props.record)));
    }
  };
}

void testDirectRootKeyedReplacementAttachesBeforeUpdatingLeaf()
{
  KeyedRootWalkRecord record;
  NullScenePlatformController platform;
  Scene scene(new BoundaryDefinition<KeyedRootWalkBoundary::Props, KeyedRootWalkBoundary>(
      KeyedRootWalkBoundary::Props(&record)));
  scene.mount(&platform);
  scene.updateAttached(true);
  LOKA_VERIFY(record.leaves[0].constructions == 1 && record.leaves[0].attaches == 1);
  LOKA_VERIFY(record.leaves[1].constructions == 0);

  setMatchState(record.key, 1);
  // State commit may apply the replacement synchronously before this drain.
  if (scene.hasPendingInvalidation())
    scene.flushInvalidation();
  const KeyedRootLeafEvents &incoming = record.leaves[1];
  std::fprintf(stderr, "direct-root Keyed replacement: ATTACH = %d, UPDATE = %d\n",
               incoming.attaches, incoming.updates);
  LOKA_VERIFY(incoming.constructions == 1);
  LOKA_VERIFY(incoming.attaches == 1 && incoming.updates == 0);

  scene.requestInvalidate(NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
  std::fprintf(stderr, "direct-root Keyed next refresh: ATTACH = %d, UPDATE = %d\n",
               incoming.attaches, incoming.updates);
  LOKA_VERIFY(incoming.constructions == 1);
  LOKA_VERIFY(incoming.attaches == 1 && incoming.updates == 1);
}

/** Characterization: CHILD dirt alone cannot redeclare a plain root. */
void testPlainRootChildDirtWithoutSeatOnlyWalks()
{
  PlainRootRecord record;
  SceneTestSupport::RecordingPlatformController platform;
  Scene scene(new NodeDefinition<PlainRootProps, PlainRootNode>(PlainRootProps(&record)));
  scene.mount(&platform);
  scene.updateAttached(true);
  BoundaryNode *wrapper = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  Node *plainRoot = wrapper->childrenHead();
  LOKA_VERIFY(plainRoot && !plainRoot->asBoundary());
  for (int update = 1; update <= 2; ++update)
  {
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
    LOKA_VERIFY(wrapper->childrenHead() == plainRoot && record.compositions == 1);
    LOKA_VERIFY(record.rootUpdates == update && record.childUpdates == update);
    const PlatformApplyPlan &plan = loka::dsl::testing::SceneTestAccess::lastApplyPlan(scene);
    LOKA_VERIFY(!plan.structureChanged);
  }
  // Allocation recovery keeps its platform meaning without redeclaring the root.
  scene.noteComposeAllocationFailure();
  scene.requestInvalidate(NODE_DIRTY_PROPS);
  LOKA_VERIFY(scene.flushInvalidation());
  LOKA_VERIFY(record.compositions == 1 && wrapper->childrenHead() == plainRoot);
  LOKA_VERIFY(record.rootUpdates == 3 && record.childUpdates == 3);
  const PlatformApplyPlan &recoveryPlan = loka::dsl::testing::SceneTestAccess::lastApplyPlan(scene);
  const bool fullProjection = platform.changeAt(platform.changeCount() - 1).fullRebuild;
  LOKA_VERIFY(recoveryPlan.structureChanged && fullProjection);
}
