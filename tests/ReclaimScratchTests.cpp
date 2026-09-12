#include "ReclaimScratchTests.hpp"
#include "support/TestVerify.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/boundary/detail/NodePartition.hpp"
#include <cstdlib>
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif
#ifdef LOKA_RECLAIM_GAUGE_PIN
#include "support/ReclaimGauge.hpp"
#endif

using namespace loka::app::scene;
using namespace loka::app::scene::detail;
namespace
{
  struct BoundaryProbe : BoundaryNode
  {
    using BoundaryNode::retireDetachedNode;
    virtual void composeWithContext(ComponentContext &, ComposeEvent) {}
  };
  struct Log
  {
    bool alive[300];
    int order[300];
    size_t count;
    Log()
        : count(0)
    {
      for (size_t i = 0; i < 300; ++i)
        this->alive[i] = false;
    }
  };
  struct Probe : NestableNode
  {
    Log &log;
    int id, provider;
    Probe(Log &l, int n, int p)
        : log(l),
          id(n),
          provider(p)
    {
      this->log.alive[n] = true;
    }
    ~Probe()
    {
      LOKA_VERIFY(this->provider < 0 || this->log.alive[this->provider]);
      LOKA_VERIFY(this->log.alive[this->id]);
      this->log.alive[this->id] = false;
      this->log.order[this->log.count++] = this->id;
    }
  };
  struct BoundaryOwnershipProbe : BoundaryProbe
  {
    Log &log;
    explicit BoundaryOwnershipProbe(Log &value)
        : log(value)
    {
      this->log.alive[0] = true;
    }
    ~BoundaryOwnershipProbe()
    {
      LOKA_VERIFY(this->log.count == 1 && this->log.order[0] == 1);
      // Pure queries stay in assert: a LOKA_VERIFY here would register
      // childrenCount/childrenHead/isArenaAllocated as load-bearing names
      // project-wide (check_test_asserts roster, #684).
      assert(this->childrenCount() == 2); // loka-assert-ok: pure child-count query
      Node *first = this->childrenHead();
      assert(first && first->isArenaAllocated());
      assert(first->nextInComposition && !first->nextInComposition->isArenaAllocated());
      assert(!first->nextInComposition->nextInComposition);
      (void)first;
      this->log.alive[0] = false;
      this->log.order[this->log.count++] = 0;
    }
  };
  class Capture
  {
  public:
    Capture()
    {
#ifdef LOKA_RECLAIM_GAUGE_PIN
      reclaimGaugeBegin();
#endif
    }
    ~Capture()
    {
#ifdef LOKA_RECLAIM_GAUGE_PIN
      reclaimGaugeEnd();
#endif
    }
  };
  bool legacy()
  {
    return std::getenv("LOKA_RECLAIM_BASELINE") != 0;
  }
  Probe *heap(Log &log, int id, int provider)
  {
    void *raw = loka::core::LokaAllocRaw(sizeof(Probe), NodeHeapAllocationSite());
    LOKA_VERIFY(raw);
    Probe *node = new (raw) Probe(log, id, provider);
    node->setGateAllocated(true);
    return node;
  }
  enum Door
  {
    BOUNDARY,
    GENERATION,
    PARTITION
  };
  void reclaim(Door door, bool deep)
  {
    Log log;
    BoundaryProbe boundary;
    NodeArena &arena = *boundary.nodeArena();
    NodePartition partition;
    NodeArena::RetiredNodeGeneration generation;
    const int count = deep ? 64 : 194;
    const NodeSlotLayout layout = NodeSlotLayout::of<Probe>(count);
    if (door == PARTITION)
    {
      LOKA_VERIFY(partition.boot(&layout, 1, count));
      if (!legacy())
        LOKA_VERIFY(partition.reserveReclaimScratch(256));
    }
    else
    {
      arena.reserve((sizeof(Probe) + AlignOf<Probe>::value) * count);
      if (!legacy())
        LOKA_VERIFY(arena.reserveReclaimScratch(256));
    }
    Probe *nodes[194];
    for (int i = 0; i < count; ++i)
    {
      const int parent = i == 0 ? -1 : (deep ? i - 1 : (i == 1 ? 0 : ((i - 2) % 3 == 0 ? 1 : i - 1)));
      // Heap tails preserve the generation path's heap-before-arena contract.
      const bool resident = deep ? i < count / 2 : (i < 2 || (i - 2) % 3 == 0);
      if (resident)
      {
        void *raw =
            door == PARTITION ? partition.allocate(layout) : arena.allocate(sizeof(Probe), AlignOf<Probe>::value);
        LOKA_VERIFY(raw);
        nodes[i] = new (raw) Probe(log, i, parent);
        if (door == PARTITION)
          LOKA_VERIFY(partition.registerNode(nodes[i], parent < 0 ? 0 : nodes[parent]));
        else
          arena.registerNode(nodes[i]);
      }
      else
      {
        const bool plain = i == count - 1 || (!deep && (i - 2) % 3 == 2);
        nodes[i] = plain ? new Probe(log, i, parent) : heap(log, i, parent);
        if (door == PARTITION && !plain)
          LOKA_VERIFY(partition.registerHeap(nodes[i], nodes[parent]));
      }
      if (parent >= 0)
        nodes[parent]->addChild(nodes[i]);
    }
    ComponentContext context;
    if (door == BOUNDARY)
      boundary.retireDetachedNode(context, nodes[0]);
    if (door == GENERATION)
      LOKA_VERIFY(arena.detachRetiredGeneration(generation));
    {
      Capture capture;
      switch (door)
      {
      case BOUNDARY:
        boundary.drainRetiredSubtreesAtNextTrackerRun();
        break;
      case GENERATION:
        if (legacy())
          NodeArena::destroyRetiredGeneration(generation);
        else
          LOKA_VERIFY(NodeArena::destroyRetiredGeneration(generation, arena.reclaimScratch()));
        break;
      case PARTITION:
        LOKA_VERIFY(partition.destroy(nodes[0], layout));
        break;
      }
    }
    LOKA_VERIFY(log.count == size_t(count));
    if (deep)
      for (int i = 0; i < count; ++i)
        LOKA_VERIFY(log.order[i] == count - i - 1);
    else if (door == GENERATION)
    {
      for (int i = 0; i < 64; ++i)
      {
        LOKA_VERIFY(log.order[2 * i] == 4 + 3 * i);
        LOKA_VERIFY(log.order[2 * i + 1] == 3 + 3 * i);
        LOKA_VERIFY(log.order[128 + i] == 191 - 3 * i);
      }
      LOKA_VERIFY(log.order[192] == 1 && log.order[193] == 0);
    }
    else
    {
      for (int i = 0; i < 64; ++i)
        for (int j = 0; j < 3; ++j)
          LOKA_VERIFY(log.order[3 * i + j] == 4 + 3 * i - j);
      LOKA_VERIFY(log.order[192] == 1 && log.order[193] == 0);
    }
  }

  void overflow(Door door)
  {
    Log log;
    BoundaryProbe boundary;
    NodePartition partition;
    NodeArena &arena = *boundary.nodeArena();
    const NodeSlotLayout layout = NodeSlotLayout::of<Probe>(1);
    LOKA_VERIFY(partition.boot(&layout, 1));
    LOKA_VERIFY(partition.reserveReclaimScratch(2));
    LOKA_VERIFY(arena.reserveReclaimScratch(2));
    void *raw = partition.allocate(layout);
    LOKA_VERIFY(raw);
    Probe *root = new (raw) Probe(log, 0, -1);
    LOKA_VERIFY(partition.registerNode(root, 0));
    if (door != PARTITION)
    {
      LOKA_VERIFY(partition.destroy(root, layout));
      log.count = 0;
      root = heap(log, 0, -1);
    }
    root->addChild(heap(log, 1, 0));
    root->addChild(heap(log, 2, 0));
    ComponentContext context;
    NodeArena::RetiredNodeGeneration gen;
    if (door == BOUNDARY)
      boundary.retireDetachedNode(context, root);
    if (door == GENERATION)
      gen.heapRoots.push_back(root);
    {
      Capture capture;
      switch (door)
      {
      case BOUNDARY:
        boundary.drainRetiredSubtreesAtNextTrackerRun();
        break;
      case GENERATION:
        LOKA_VERIFY(!NodeArena::destroyRetiredGeneration(gen, arena.reclaimScratch()));
        break;
      case PARTITION:
        LOKA_VERIFY(!partition.destroy(root, layout));
        break;
      }
    }
    LOKA_VERIFY(log.count == 0 && root->childrenCount() == 2);
    LOKA_VERIFY(log.alive[0] && log.alive[1] && log.alive[2]);
    // Restore a fitting subtree using the ordinary caller-owned edge door;
    // retry proves refusal preserved the queue, provenance and resident rows.
    Node *children = root->detachChildren();
    Node *second = children->nextInComposition;
    children->nextInComposition = 0;
    DestroyHeapNode(second);
    root->addChild(children);
    {
      Capture capture;
      switch (door)
      {
      case BOUNDARY:
        boundary.drainRetiredSubtreesAtNextTrackerRun();
        break;
      case GENERATION:
        LOKA_VERIFY(NodeArena::destroyRetiredGeneration(gen, arena.reclaimScratch()));
        break;
      case PARTITION:
        LOKA_VERIFY(partition.destroy(root, layout));
        break;
      }
    }
    LOKA_VERIFY(log.count == 3);
  }
} // namespace
void testReclaimScratchBoundaryWide()
{
  reclaim(BOUNDARY, false);
}
void testReclaimScratchBoundaryDeep()
{
  reclaim(BOUNDARY, true);
}
void testReclaimScratchGenerationWide()
{
  reclaim(GENERATION, false);
}
void testReclaimScratchGenerationDeep()
{
  reclaim(GENERATION, true);
}
void testReclaimScratchPartitionWide()
{
  reclaim(PARTITION, false);
}
void testReclaimScratchPartitionDeep()
{
  reclaim(PARTITION, true);
}
void testReclaimScratchOverflow()
{
  for (int i = BOUNDARY; i <= PARTITION; ++i)
  {
#ifdef NDEBUG
    overflow(static_cast<Door>(i));
#elif defined(__linux__) && !defined(__SANITIZE_ADDRESS__)
    const pid_t child = fork();
    LOKA_VERIFY(child >= 0);
    if (child == 0)
    {
      overflow(static_cast<Door>(i));
      _exit(0);
    }
    int status = 0;
    LOKA_VERIFY(waitpid(child, &status, 0) == child);
    LOKA_VERIFY(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
#else
    (void)&overflow;
#endif
  }
}
void testReclaimScratchNestedBoundary()
{
  Log log;
  BoundaryProbe owner;
  LOKA_VERIFY(owner.nodeArena()->reserveReclaimScratch(2));
  BoundaryProbe *nested = new BoundaryProbe();
  ComponentContext context;
  nested->retireDetachedNode(context, heap(log, 0, -1));
  Probe *root = heap(log, 1, -1);
  root->addChild(nested);
  owner.retireDetachedNode(context, root);
  owner.drainRetiredSubtreesAtNextTrackerRun();
  LOKA_VERIFY(log.count == 2 && log.order[0] == 0 && log.order[1] == 1);
}
void testReclaimScratchPartitionUnattached()
{
  Log log;
  NodePartition partition;
  const NodeSlotLayout layout = NodeSlotLayout::of<Probe>(2);
  LOKA_VERIFY(partition.boot(&layout, 1, 1));
  LOKA_VERIFY(partition.reserveReclaimScratch(3));
  void *a = partition.allocate(layout);
  void *b = partition.allocate(layout);
  LOKA_VERIFY(a && b);
  Probe *root = new (b) Probe(log, 0, -1);
  Probe *orphan = new (a) Probe(log, 1, 0);
  LOKA_VERIFY(partition.registerNode(root, 0));
  LOKA_VERIFY(partition.registerNode(orphan, root));
  LOKA_VERIFY(partition.registerHeap(heap(log, 2, 1), orphan));
  {
    Capture capture;
    LOKA_VERIFY(partition.destroy(root, layout));
  }
  LOKA_VERIFY(log.count == 3 && log.order[0] == 2 && log.order[1] == 1 && log.order[2] == 0);
}

void testReclaimScratchPartitionBoundaryChild()
{
  Log log;
  NodePartition partition;
  const NodeSlotLayout layouts[] = {NodeSlotLayout::of<BoundaryProbe>(1), NodeSlotLayout::of<Probe>(1)};
  LOKA_VERIFY(partition.boot(layouts, 2));
  if (!legacy())
    LOKA_VERIFY(partition.reserveReclaimScratch(2));
  void *parentStorage = partition.allocate(layouts[0]);
  void *childStorage = partition.allocate(layouts[1]);
  LOKA_VERIFY(parentStorage && childStorage);
  BoundaryProbe *parent = new (parentStorage) BoundaryProbe();
  Probe *child = new (childStorage) Probe(log, 0, -1);
  LOKA_VERIFY(partition.registerNode(parent, 0));
  LOKA_VERIFY(partition.registerNode(child, parent));
  parent->addChild(child);
  LOKA_VERIFY(partition.destroy(parent, layouts[0]));
  LOKA_VERIFY(log.count == 1 && !log.alive[0]);
}

void testReclaimScratchPartitionBoundaryKeepsOwnChildren()
{
  Log log;
  NodePartition partition;
  const NodeSlotLayout layouts[] = {NodeSlotLayout::of<BoundaryOwnershipProbe>(1), NodeSlotLayout::of<Probe>(1)};
  LOKA_VERIFY(partition.boot(layouts, 2));
  LOKA_VERIFY(partition.reserveReclaimScratch(2));
  void *parentStorage = partition.allocate(layouts[0]);
  void *childStorage = partition.allocate(layouts[1]);
  LOKA_VERIFY(parentStorage && childStorage);
  BoundaryOwnershipProbe *parent = new (parentStorage) BoundaryOwnershipProbe(log);
  Probe *child = new (childStorage) Probe(log, 1, 0);
  LOKA_VERIFY(partition.registerNode(parent, 0));
  LOKA_VERIFY(partition.registerNode(child, parent));
  NodeArena &arena = *parent->nodeArena();
  arena.reserve(sizeof(Probe) + AlignOf<Probe>::value);
  void *ownStorage = arena.allocate(sizeof(Probe), AlignOf<Probe>::value);
  LOKA_VERIFY(ownStorage);
  Probe *ownArenaChild = new (ownStorage) Probe(log, 2, -1);
  arena.registerNode(ownArenaChild);
  parent->addChild(ownArenaChild);
  parent->addChild(child);
  parent->addChild(heap(log, 3, -1));
  // The two-frame budget excludes children left to the nested landlord.
  LOKA_VERIFY(partition.destroy(parent, layouts[0]));
  LOKA_VERIFY(log.count == 4 && log.order[0] == 1 && log.order[1] == 0);
  LOKA_VERIFY(!log.alive[0] && !log.alive[1] && !log.alive[2] && !log.alive[3]);
}
