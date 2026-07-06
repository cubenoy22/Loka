#include "DefinitionCloneTests.hpp"
#include <cassert>
#include <cstdio>
#include "core/State.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/node/Conditional.hpp"

// Regression pin for the ConditionalDefinition dangling-branch fix (issue #47,
// docs/TODO.md "DSL definition lifetime safety"): ConditionalDefinition must own
// deep copies of its branch definitions so that stack-local/temporary branch
// definitions can be destroyed without leaving the conditional (or its clones)
// pointing at freed memory. Pre-fix code fails this under ASan.

// ============================================================
// Probe definition/node with liveness counters
// ============================================================

namespace
{

  class CloneProbeNode;
  struct CloneProbeTypeTag
  {
  };

  static int g_probePropsAlive = 0;
  static int g_probeNodesAlive = 0;
  static int g_probeNodesCreated = 0;

  struct CloneProbeProps : public loka::app::scene::NodePropsBase<CloneProbeProps>
  {
    typedef CloneProbeTypeTag TypeTag;
    typedef CloneProbeNode NodeType;
    CloneProbeProps()
    {
      ++g_probePropsAlive;
    }
    CloneProbeProps(const CloneProbeProps &other)
        : loka::app::scene::NodePropsBase<CloneProbeProps>(other)
    {
      ++g_probePropsAlive;
    }
    ~CloneProbeProps()
    {
      --g_probePropsAlive;
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
  };

  class CloneProbeNode : public loka::app::scene::Node
  {
  public:
    typedef CloneProbeTypeTag TypeTag;
    CloneProbeProps props;
    CloneProbeNode(const CloneProbeProps &p)
        : props(p)
    {
      ++g_probeNodesAlive;
      ++g_probeNodesCreated;
    }
    virtual ~CloneProbeNode()
    {
      --g_probeNodesAlive;
    }
  };

  struct CloneProbeDefinition : public loka::app::scene::NodeDefinition<CloneProbeProps, CloneProbeNode>
  {
    CloneProbeDefinition()
        : loka::app::scene::NodeDefinition<CloneProbeProps, CloneProbeNode>()
    {
    }
  };

  struct NullCloneProbeDefinition : public CloneProbeDefinition
  {
    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return 0;
    }
  };
} // namespace

// ============================================================
// Tests
// ============================================================

void testConditionalDefinitionCloneOwnership()
{
  printf("\n==== [testConditionalDefinitionCloneOwnership] start ====\n");

  using loka::app::scene::ConditionalDefinition;
  using loka::app::scene::ConditionalProps;

  // --- the definition owns deep copies of its branch definitions ---
  // The branch definition is stack-local and destroyed before the conditional
  // is cloned or materialized; the owned clones must keep working.
  {
    loka::core::MutableState<bool> cond(true);
    ConditionalDefinition *conditional = 0;
    {
      CloneProbeDefinition trueBranch;
      conditional = new ConditionalDefinition(ConditionalProps(&cond, &trueBranch, 0));
    }
    // trueBranch is gone. Clone the conditional, then destroy the original:
    // the clone must own its own independent branch copy.
    loka::app::scene::NodeDefinitionBase *copy = conditional->clone();
    delete conditional;

    int createdBefore = g_probeNodesCreated;
    loka::app::scene::Node *node = copy->create();
    assert(node != 0);
    assert(g_probeNodesCreated > createdBefore); // true branch materialized from the owned clone
    delete node;
    delete copy;
  }

  // --- assignment deep-copies; self-assignment is safe ---
  {
    loka::core::MutableState<bool> cond(false);
    {
      CloneProbeDefinition falseBranch;
      ConditionalDefinition first(ConditionalProps(&cond, 0, &falseBranch));
      ConditionalDefinition second(ConditionalProps(&cond, 0, 0));
      second = first;
      second = second; // self-assignment must not free the owned branches

      loka::app::scene::Node *node = second.create();
      assert(node != 0);
      delete node;
    }
  }

  // --- ownership balance: every owned copy destroyed exactly once ---
  // A double delete or a leaked branch clone shows up here (and under ASan).
  assert(g_probePropsAlive == 0);
  assert(g_probeNodesAlive == 0);

  printf("==== [testConditionalDefinitionCloneOwnership] end ====\n");
}

void testNestableDefinitionAssignmentRejectsNullChildClone()
{
  printf("\n==== [testNestableDefinitionAssignmentRejectsNullChildClone] start ====\n");

  using loka::app::Box;
  using loka::app::BoxDefinition;

  CloneProbeDefinition originalChild;
  BoxDefinition stableTarget = Box().padding(10) << originalChild;
  BoxDefinition sourceWithBadChild;
  sourceWithBadChild.padding(20);
  sourceWithBadChild << new NullCloneProbeDefinition();

  stableTarget = sourceWithBadChild;

  assert(stableTarget.props.padding == 10);
  assert(stableTarget.childrenCount() == 1);
  assert(stableTarget.childrenHead() != 0);
  loka::app::scene::NodeDefinitionBase *childClone = stableTarget.childrenHead()->clone();
  assert(childClone != 0);
  delete childClone;

  printf("==== [testNestableDefinitionAssignmentRejectsNullChildClone] end ====\n");
}
