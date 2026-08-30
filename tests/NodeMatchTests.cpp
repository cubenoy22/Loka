#include "NodeMatchTests.hpp"
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
#include "support/RecomposingBoundary.hpp"
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

  class MatchRootBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<MatchRootBoundaryNode,
                                                         MatchRootBoundaryProps>
  {
  public:
    explicit MatchRootBoundaryNode(const MatchRootBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<MatchRootBoundaryNode,
                                                    MatchRootBoundaryProps>(props)
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
