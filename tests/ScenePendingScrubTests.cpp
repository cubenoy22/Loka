#include "ScenePendingScrubTests.hpp"
#include <cassert>
#include <cstdio>
#include "core/State.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/projection/SceneProjectionTransaction.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/Text.hpp"
#include "testing/scene/SceneTestFlow.hpp"

// Regression pins for the pending-update scrub (#44): a structural rebuild
// that REPLACEs a subtree containing a pending nested child boundary must
// tombstone that boundary's transaction entry before the subtree is deleted,
// or the same-flush snapshot/apply walk dereferences a freed Node*. This is
// the stale-entry half of the compose/apply reentrancy bug family; the phase
// guard (#45 item 1) covers the reentrancy half, and the nested-boundary
// REPLACE machinery here is the discriminating fixture promised in that work.

// ============================================================
// Fixtures
// ============================================================

namespace
{

  using namespace loka::app::scene;
  using loka::dsl::testing::SceneTestAccess;

  enum
  {
    kSiblingTag = 1,
    kMorphTag = 2
  };

  // Selects the morph slot expansion for the NEXT root-definition clone.
  static bool g_morphToText = false;
  // Expands to two untagged children so the by-tag diff fails to build and
  // the root wrapper takes the full-rebuild fallback instead of a local
  // rebuild.
  static bool g_morphFullRebuild = false;
  // Controls whether the nested boundary requests an immediate flush for its
  // view-dirty writes. false leaves the write NextTick-pending, which is what
  // plants a pending transaction entry without running a cycle.
  static bool g_nestedFlushImmediately = false;
  static bool g_writeOnNestedDetach = false;
  static bool g_inNestedDetachWrite = false;
  static int g_nestedDetachWrites = 0;
  static int g_updateComposeDuringNestedDetachWrite = 0;

  // Nested child boundary living inside the morph slot subtree.
  class NestedChildBoundaryNode;
  typedef BoundaryPropsFor<NestedChildBoundaryNode> NestedChildProps;

  class NestedChildBoundaryNode : public BoundaryNodeFor<NestedChildBoundaryNode>
  {
  public:
    NestedChildBoundaryNode(const NestedChildProps &p)
        : BoundaryNodeFor<NestedChildBoundaryNode>(NestedChildProps(p)),
          value_()
    {
      this->state(this->value_, loka::core::String::Literal("A"));
    }

    virtual bool flushViewDirtyImmediately(NodeDirtyFlags flags) const
    {
      (void)flags;
      return g_nestedFlushImmediately;
    }

    virtual void composeNode(NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(Text(this->value_.state()));
    }

    // Observed-state write while this boundary's own DETACH teardown runs:
    // the write re-enqueues this boundary as a pending target mid-rebuild, so
    // the scrub must run after the DETACH compose to catch it.
    virtual void detachNode(NodeComposition &c)
    {
      if (g_writeOnNestedDetach && this->value_.isValid())
      {
        ++g_nestedDetachWrites;
        g_inNestedDetachWrite = true;
        this->value_.set(loka::core::String::Literal("DetachWrite"), true);
        g_inNestedDetachWrite = false;
      }
      BoundaryNodeFor<NestedChildBoundaryNode>::detachNode(c);
    }

  private:
    NodeState<loka::core::String> value_;
  };

  inline BoundaryDefinition<NestedChildProps, NestedChildBoundaryNode> NestedChild()
  {
    return Boundary<NestedChildBoundaryNode>();
  }

  // Reentry detector that survives every morph: an UPDATE compose reaching it
  // while the nested boundary's DETACH-handler write is in flight means the
  // write synchronously re-entered the refresh loop.
  class SiblingBoundaryNode;
  typedef BoundaryPropsFor<SiblingBoundaryNode> SiblingProps;

  class SiblingBoundaryNode : public BoundaryNodeFor<SiblingBoundaryNode>
  {
  public:
    SiblingBoundaryNode(const SiblingProps &p)
        : BoundaryNodeFor<SiblingBoundaryNode>(SiblingProps(p))
    {
    }

    virtual void composeNode(NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(Text("sibling"));
    }

    virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
    {
      if (event == COMPOSE_EVENT_UPDATE && g_inNestedDetachWrite)
      {
        ++g_updateComposeDuringNestedDetachWrite;
      }
      BoundaryNodeFor<SiblingBoundaryNode>::composeWithContext(context, event);
    }
  };

  inline BoundaryDefinition<SiblingProps, SiblingBoundaryNode> SiblingBoundary()
  {
    return Boundary<SiblingBoundaryNode>();
  }

  // Scene-root definition whose expansion is decided at declare time: the
  // root wrapper re-clones its definition every UPDATE cycle, so clone() is
  // the hook that changes the morph slot's node TYPE at a stable tag and
  // drives the local-rebuild REPLACE path.
  struct MorphRootDefinition : public loka::app::FragmentDefinition
  {
    virtual NodeDefinitionBase *clone() const
    {
      loka::app::FragmentDefinition *expanded = new loka::app::FragmentDefinition();
      if (g_morphFullRebuild)
      {
        loka::app::TextDefinition a("full-a");
        loka::app::TextDefinition b("full-b");
        expanded->addChild(&a);
        expanded->addChild(&b);
        return expanded;
      }
      {
        BoundaryDefinition<SiblingProps, SiblingBoundaryNode> sibling = SiblingBoundary();
        sibling.setNodeTag(kSiblingTag);
        expanded->addChild(&sibling);
      }
      if (g_morphToText)
      {
        loka::app::TextDefinition text("morphed");
        text.setNodeTag(kMorphTag);
        expanded->addChild(&text);
      }
      else
      {
        loka::app::FragmentDefinition host;
        host << NestedChild();
        host.setNodeTag(kMorphTag);
        expanded->addChild(&host);
      }
      return expanded;
    }
  };

  class ScrubTestController : public IPlatformController
  {
  public:
    ScrubTestController()
        : destroyed_(false)
    {
    }
    virtual void onChange(Node *, NodeDirtyFlags, bool) {}
    virtual void synchronize() {}
    virtual bool hasPendingSync() const
    {
      return false;
    }
    virtual void destroy()
    {
      destroyed_ = true;
    }

    bool destroyed_;
  };

  // Controller that morphs the scene from inside the apply window: onChange
  // fires while the platform apply is in flight, and the write both flips the
  // morph slot and requests a structure update. Pre-guard this synchronously
  // re-entered refresh mid-apply with a pending local rebuild -- the reentry
  // scenario the minimal #45 fixture could not reach.
  class ApplyWindowMorphController : public IPlatformController
  {
  public:
    ApplyWindowMorphController()
        : sibling_(0),
          morphOnNextChange_(false),
          onChangeCalls_(0),
          nestedCallsDuringMorphWrite_(-1),
          destroyed_(false)
    {
    }

    virtual void onChange(Node *, NodeDirtyFlags, bool)
    {
      ++onChangeCalls_;
      if (morphOnNextChange_ && sibling_)
      {
        morphOnNextChange_ = false;
        g_morphToText = true;
        const int callsBeforeWrite = onChangeCalls_;
        sibling_->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
        // A synchronous reentry runs refresh+apply inside the write, so a
        // nested onChange makes this delta nonzero.
        nestedCallsDuringMorphWrite_ = onChangeCalls_ - callsBeforeWrite;
      }
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const
    {
      return false;
    }
    virtual void destroy()
    {
      destroyed_ = true;
    }

    BoundaryNode *sibling_;
    bool morphOnNextChange_;
    int onChangeCalls_;
    int nestedCallsDuringMorphWrite_;
    bool destroyed_;
  };

  Node *findNodeByTag(Node *node, NodeTag tag)
  {
    if (!node)
    {
      return 0;
    }
    if (node->nodeTag() == tag)
    {
      return node;
    }
    INestable *nestable = node->asNestable();
    if (!nestable)
    {
      return 0;
    }
    loka::dsl::CompositionCursor<Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (Node *child = it.next(); child; child = it.next())
    {
      Node *found = findNodeByTag(child, tag);
      if (found)
      {
        return found;
      }
    }
    return 0;
  }

  BoundaryNode *findFirstBoundaryUnder(Node *node)
  {
    if (!node)
    {
      return 0;
    }
    INestable *nestable = node->asNestable();
    if (!nestable)
    {
      return 0;
    }
    loka::dsl::CompositionCursor<Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (Node *child = it.next(); child; child = it.next())
    {
      BoundaryNode *boundary = child->asBoundary();
      if (boundary)
      {
        return boundary;
      }
      boundary = findFirstBoundaryUnder(child);
      if (boundary)
      {
        return boundary;
      }
    }
    return 0;
  }

  void morph(Scene &scene, bool toText)
  {
    g_morphToText = toText;
    scene.invalidate(NODE_DIRTY_CHILD);
  }

  void resetScrubGlobals()
  {
    g_morphToText = false;
    g_morphFullRebuild = false;
    g_nestedFlushImmediately = false;
    g_writeOnNestedDetach = false;
    g_inNestedDetachWrite = false;
    g_nestedDetachWrites = 0;
    g_updateComposeDuringNestedDetachWrite = 0;
  }

  // Cycles the morph slot text -> host so the rebuilt host subtree is
  // heap-allocated: local-rebuild creation runs without a boundary arena, so
  // the REPLACE that later retires the subtree really deletes it, which is
  // what makes a pre-fix stale entry an observable use-after-free under ASan.
  BoundaryNode *rebuildHostOnHeap(Scene &scene, Node *root)
  {
    morph(scene, true);
    morph(scene, false);
    Node *host = findNodeByTag(root, kMorphTag);
    assert(host && host->asNestable());
    assert(!host->isArenaAllocated());
    BoundaryNode *nested = findFirstBoundaryUnder(host);
    assert(nested);
    return nested;
  }

} // namespace

// ============================================================
// Tests
// ============================================================

static void test_transaction_removeTarget_tombstones()
{
  using loka::app::scene::Node;
  using loka::app::scene::SceneProjectionTransaction;

  loka::app::FragmentProps props;
  loka::app::FragmentNode a(props);
  loka::app::FragmentNode b(props);
  loka::app::FragmentNode c(props);

  SceneProjectionTransaction transaction;
  assert(!transaction.removeTarget(&a)); // empty transaction
  transaction.enqueue(&a, loka::app::scene::NODE_DIRTY_PROPS);
  transaction.enqueue(&b, loka::app::scene::NODE_DIRTY_CHILD);
  transaction.enqueue(&c, loka::app::scene::NODE_DIRTY_LAYOUT);
  assert(transaction.targetCount() == 3);

  // Remove head: entry tombstones in place, iteration and the aggregates
  // skip it, and the address no longer resolves.
  assert(transaction.removeTarget(&a));
  assert(transaction.targetCount() == 2);
  assert(transaction.hasPending());
  assert(transaction.dirtyFlagsForNode(&a) == loka::app::scene::NODE_DIRTY_NONE);
  assert(transaction.aggregateDirtyFlags()
         == static_cast<loka::app::scene::NodeDirtyFlags>(loka::app::scene::NODE_DIRTY_CHILD
                                                          | loka::app::scene::NODE_DIRTY_LAYOUT));
  assert(!transaction.removeTarget(&a)); // already tombstoned
  assert(!transaction.removeTarget(static_cast<const Node *>(0)));

  // Iteration exposes the tombstone as a null node; live entries keep order.
  {
    SceneProjectionTransaction::ConstIterator it = transaction.targetsBegin();
    assert(it.isValid() && it.node() == 0);
    it.next();
    assert(it.isValid() && it.node() == &b);
    it.next();
    assert(it.isValid() && it.node() == &c);
    it.next();
    assert(!it.isValid());
  }

  // Remove tail, then middle: only the middle entry stays live.
  assert(transaction.removeTarget(&c));
  assert(transaction.targetCount() == 1);
  assert(transaction.dirtyFlagsForNode(&b) == loka::app::scene::NODE_DIRTY_CHILD);

  // Re-enqueue after tombstoning creates a fresh live entry.
  transaction.enqueue(&a, loka::app::scene::NODE_DIRTY_PROPS);
  assert(transaction.targetCount() == 2);
  assert(transaction.dirtyFlagsForNode(&a) == loka::app::scene::NODE_DIRTY_PROPS);

  // All-tombstoned reports empty even though entries are still linked.
  assert(transaction.removeTarget(&a));
  assert(transaction.removeTarget(&b));
  assert(!transaction.hasPending());
  assert(transaction.targetCount() == 0);
  assert(transaction.aggregateDirtyFlags() == loka::app::scene::NODE_DIRTY_NONE);

  transaction.clear();
  assert(!transaction.targetsBegin().isValid());
}

// The #44 failure sequence: dirty a nested child boundary (NextTick-pending),
// then REPLACE the subtree containing it in the same flush. Pre-fix the
// snapshot/apply walk dereferences the freed boundary (ASan
// heap-use-after-free); post-fix the entry is tombstoned and the flush
// completes with the expected tree.
static void test_scrub_replace_drops_pending_nested_boundary()
{
  using loka::app::scene::BoundaryNode;
  using loka::app::scene::Node;
  using loka::app::scene::Scene;

  resetScrubGlobals();
  Scene scene(new MorphRootDefinition());
  ScrubTestController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  Node *root = SceneTestAccess::rootNode(scene);
  assert(root);
  BoundaryNode *nested = rebuildHostOnHeap(scene, root);

  nested->markViewDirty(loka::app::scene::NODE_DIRTY_PROPS);
  assert(SceneTestAccess::projectionTransactionDirtyFlagsForNode(scene, nested)
         != loka::app::scene::NODE_DIRTY_NONE);
  const Node *staleNested = nested;

  morph(scene, true);

  assert(SceneTestAccess::projectionTransactionDirtyFlagsForNode(scene, staleNested)
         == loka::app::scene::NODE_DIRTY_NONE);
  assert(!SceneTestAccess::projectionTransactionHasPending(scene));
  Node *morphed = findNodeByTag(root, kMorphTag);
  assert(morphed && !morphed->asBoundary() && !morphed->asNestable());
  Node *sibling = findNodeByTag(root, kSiblingTag);
  assert(sibling && sibling->asBoundary());

  scene.updateAttached(false);
  scene.unmount();
}

// The interplay of both bug halves: the nested boundary's DETACH handler
// writes an observed state while its subtree is being REPLACEd. The phase
// guard must keep the write from re-entering the refresh loop (no UPDATE
// compose lands on the surviving sibling mid-write), and the scrub must drop
// the entry that this very write re-enqueues after the pre-existing entries
// were already scrubbed -- which is why the scrub runs after the DETACH
// compose, not before it.
static void test_scrub_detach_write_reenqueue_and_no_reentry()
{
  using loka::app::scene::BoundaryNode;
  using loka::app::scene::Node;
  using loka::app::scene::Scene;

  resetScrubGlobals();
  Scene scene(new MorphRootDefinition());
  ScrubTestController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  Node *root = SceneTestAccess::rootNode(scene);
  assert(root);
  BoundaryNode *nested = rebuildHostOnHeap(scene, root);
  const Node *staleNested = nested;

  g_writeOnNestedDetach = true;
  g_nestedFlushImmediately = true;
  morph(scene, true);
  g_writeOnNestedDetach = false;
  g_nestedFlushImmediately = false;

  assert(g_nestedDetachWrites == 1);
  assert(g_updateComposeDuringNestedDetachWrite == 0);
  assert(SceneTestAccess::projectionTransactionDirtyFlagsForNode(scene, staleNested)
         == loka::app::scene::NODE_DIRTY_NONE);
  Node *morphed = findNodeByTag(root, kMorphTag);
  assert(morphed && !morphed->asBoundary() && !morphed->asNestable());

  scene.updateAttached(false);
  scene.unmount();
}

// Same failure mode through the third deletion path (Codex P1 on the PR):
// when the child diff cannot be built, the root wrapper falls back to a full
// rebuild (detachExistingChildren + clearChildren + arena clear) instead of a
// local rebuild. A pending nested boundary retired by that fallback must be
// scrubbed too.
static void test_scrub_full_rebuild_fallback_drops_pending_nested_boundary()
{
  using loka::app::scene::BoundaryNode;
  using loka::app::scene::Node;
  using loka::app::scene::Scene;

  resetScrubGlobals();
  Scene scene(new MorphRootDefinition());
  ScrubTestController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  Node *root = SceneTestAccess::rootNode(scene);
  assert(root);
  BoundaryNode *nested = rebuildHostOnHeap(scene, root);

  nested->markViewDirty(loka::app::scene::NODE_DIRTY_PROPS);
  assert(SceneTestAccess::projectionTransactionDirtyFlagsForNode(scene, nested)
         != loka::app::scene::NODE_DIRTY_NONE);
  const Node *staleNested = nested;

  // Two untagged children make the by-tag diff unbuildable, so this update
  // takes the full-rebuild fallback and deletes the heap-allocated host
  // subtree containing the pending boundary.
  g_morphFullRebuild = true;
  scene.invalidate(loka::app::scene::NODE_DIRTY_CHILD);

  assert(SceneTestAccess::projectionTransactionDirtyFlagsForNode(scene, staleNested)
         == loka::app::scene::NODE_DIRTY_NONE);
  assert(!SceneTestAccess::projectionTransactionHasPending(scene));
  assert(!findNodeByTag(root, kMorphTag)); // old tagged tree fully retired

  scene.updateAttached(false);
  scene.unmount();
}

// The discriminating fixture promised with the phase-guard work (#45 item 1):
// a structure-changing write lands inside the apply window while a nested
// boundary is a pending update target and the write flips the morph slot, so
// a synchronous reentry would run a REPLACE local rebuild mid-apply --
// deleting the pending subtree and clearing the transaction under the outer
// apply's cursor. With the guard (markViewDirty phase check + NextTickTracker
// holding inProgress_ across apply) the write defers and the flush stays
// clean; without it this test crashes or observes a nested onChange.
static void test_apply_window_morph_write_defers()
{
  using loka::app::scene::BoundaryNode;
  using loka::app::scene::Node;
  using loka::app::scene::Scene;

  resetScrubGlobals();
  Scene scene(new MorphRootDefinition());
  ApplyWindowMorphController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  Node *root = SceneTestAccess::rootNode(scene);
  assert(root);
  BoundaryNode *nested = rebuildHostOnHeap(scene, root);
  Node *sibling = findNodeByTag(root, kSiblingTag);
  assert(sibling && sibling->asBoundary());

  // Plant a pending entry for the nested boundary, then run a flush whose
  // apply window performs the morph write.
  platform.sibling_ = sibling->asBoundary();
  platform.morphOnNextChange_ = true;
  nested->markViewDirty(loka::app::scene::NODE_DIRTY_PROPS);
  scene.flushInvalidation();

  assert(!platform.morphOnNextChange_); // the apply-window write ran
  assert(platform.nestedCallsDuringMorphWrite_ == 0);
  // Current behavior pin (matches PhaseGuardTests): the update enqueued
  // during apply is dropped by the end-of-cycle transaction clear (#45 item
  // 3), so the morph slot still hosts the nested subtree after this flush.
  Node *slot = findNodeByTag(root, kMorphTag);
  assert(slot && slot->asNestable());

  // An explicit follow-up rebuild applies the pending morph; the scrub keeps
  // the retiring subtree out of the transaction as usual.
  morph(scene, true);
  slot = findNodeByTag(root, kMorphTag);
  assert(slot && !slot->asBoundary() && !slot->asNestable());

  scene.updateAttached(false);
  scene.unmount();
}

void testScenePendingUpdateScrub()
{
  printf("\n==== [testScenePendingUpdateScrub] start ====\n");
  test_transaction_removeTarget_tombstones();
  test_scrub_replace_drops_pending_nested_boundary();
  test_scrub_full_rebuild_fallback_drops_pending_nested_boundary();
  test_scrub_detach_write_reenqueue_and_no_reentry();
  test_apply_window_morph_write_defers();
  printf("==== [testScenePendingUpdateScrub] end ====\n");
}
