#include "NodePartitionTests.hpp"
#include "support/TestVerify.hpp"
#include "app/scene/boundary/detail/NodePartition.hpp"
#include "app/scene/boundary/detail/BoundaryArena.hpp"
#include <cstdlib>
#include <cstdio>

using namespace loka::app::scene;
using namespace loka::app::scene::detail;
namespace
{
  int acquisitions = 0, frees = 0, deaths = 0;
  bool refuse = false;
  void *acquire(size_t bytes, const loka::core::LokaAllocationSite &)
  {
    ++acquisitions;
    return refuse ? 0 : std::malloc(bytes);
  }
  void release(void *p, const loka::core::LokaAllocationSite &)
  {
    ++frees;
    std::free(p);
  }
  struct Backend
  {
    Backend()
    {
      acquisitions = frees = deaths = 0;
      refuse = false;
      loka::core::LokaAllocSetBackend(acquire, release);
    }
    ~Backend()
    {
      loka::core::LokaAllocSetBackend(0, 0);
    }
  };
  struct Probe : NestableNode
  {
    Probe *provider;
    bool *alive;
    Probe(Probe *p = 0, bool *a = 0)
        : provider(p),
          alive(a)
    {
      if (a)
        *a = true;
    }
    ~Probe()
    {
      if (this->provider)
        LOKA_VERIFY(*this->provider->alive);
      if (this->alive)
        *this->alive = false;
      ++deaths;
    }
  };
  struct Wide : Probe
  {
    char padding[73];
  };
} // namespace

void testNodePartitionReuse()
{
  Backend backend;
  const NodeSlotLayout layout = NodeSlotLayout::of<Probe>(8);
  // Behavioral baseline: releaseNode destroys, but never rewinds the bump.
  // LOKA_NODE_PARTITION_BASELINE_RED runs the same reuse obligation on HEAD's arena.
  if (std::getenv("LOKA_NODE_PARTITION_BASELINE_RED"))
  {
    NodeArena arena;
    arena.reserve(sizeof(Probe) * 8);
    void *addresses[8];
    for (int i = 0; i < 8; ++i)
    {
      addresses[i] = arena.allocate(sizeof(Probe), AlignOf<Probe>::value);
      LOKA_VERIFY(addresses[i]);
      Probe *p = new (addresses[i]) Probe();
      arena.registerNode(p);
    }
    for (int i = 0; i < 8; ++i)
      LOKA_VERIFY(arena.releaseNode(static_cast<Probe *>(addresses[i])));
    LOKA_VERIFY(arena.allocate(sizeof(Probe), AlignOf<Probe>::value) != 0);
    return;
  }
  {
    NodePartition partition;
    LOKA_VERIFY(partition.boot(&layout, 1));
    LOKA_VERIFY(acquisitions == 1);
    void *addresses[8];
    for (int i = 0; i < 8; ++i)
    {
      addresses[i] = partition.allocate(layout);
      LOKA_VERIFY(addresses[i]);
      LOKA_VERIFY(partition.registerNode(new (addresses[i]) Probe(), 0));
    }
    LOKA_VERIFY(!partition.allocate(layout));
    refuse = true;
    for (int round = 0; round < 20; ++round)
    {
      for (int i = 0; i < 8; ++i)
        LOKA_VERIFY(partition.destroy(addresses[i], layout));
      bool seen[8] = {false, false, false, false, false, false, false, false};
      for (int i = 0; i < 8; ++i)
      {
        void *p = partition.allocate(layout);
        int index = 0;
        while (index < 8 && addresses[index] != p)
          ++index;
        LOKA_VERIFY(index < 8);
        LOKA_VERIFY(!seen[index]);
        seen[index] = true;
        LOKA_VERIFY(partition.registerNode(new (p) Probe(), 0));
      }
      LOKA_VERIFY(acquisitions == 1 && frees == 0);
    }
  }
  LOKA_VERIFY(deaths == 168);
  LOKA_VERIFY(frees == 1);
}

void testNodePartitionReturnValidation()
{
  Backend backend;
  NodeSlotLayout layouts[] = {NodeSlotLayout::of<Probe>(1), NodeSlotLayout::of<Wide>(1)};
  NodePartition a, b;
  LOKA_VERIFY(a.boot(layouts, 2));
  LOKA_VERIFY(b.boot(layouts, 2));
  void *foreign = b.allocate(layouts[0]);
  LOKA_VERIFY(foreign && b.registerNode(new (foreign) Probe(), 0));
  void *p = a.allocate(layouts[0]);
  LOKA_VERIFY(p);
  LOKA_VERIFY(!a.cancel(static_cast<char *>(p) + 1, layouts[0]));
  LOKA_VERIFY(!b.cancel(p, layouts[0]));
  LOKA_VERIFY(!a.cancel(p, layouts[1]));
  LOKA_VERIFY(!a.destroy(p, layouts[0]));
  LOKA_VERIFY(a.registerNode(new (p) Probe(), 0));
  LOKA_VERIFY(!a.registerNode(static_cast<Probe *>(p), 0));
  LOKA_VERIFY(!a.cancel(p, layouts[0]));
  LOKA_VERIFY(!b.destroy(p, layouts[0]));
  LOKA_VERIFY(!a.destroy(p, layouts[1]));
  LOKA_VERIFY(!a.destroy(static_cast<char *>(p) + 1, layouts[0]));
  LOKA_VERIFY(a.destroy(p, layouts[0]));
  LOKA_VERIFY(!a.destroy(p, layouts[0]));
  LOKA_VERIFY(!a.cancel(p, layouts[0]));
  void *q = a.allocate(layouts[1]);
  LOKA_VERIFY(q && q != p);
  LOKA_VERIFY(!a.allocate(layouts[1]));
  LOKA_VERIFY(a.cancel(q, layouts[1]));
  LOKA_VERIFY(a.allocate(layouts[1]) == q);
  LOKA_VERIFY(a.cancel(q, layouts[1]));
  LOKA_VERIFY(deaths == 1);
}

void testNodePartitionTopology()
{
  Backend backend;
  bool parentAlive = false, childAlive = false, orphanAlive = false;
  const NodeSlotLayout layout = NodeSlotLayout::of<Probe>(3);
  {
    NodePartition partition;
    LOKA_VERIFY(partition.boot(&layout, 1, 1));
    void *childStorage = partition.allocate(layout);
    void *parentStorage = partition.allocate(layout);
    void *orphanStorage = partition.allocate(layout);
    LOKA_VERIFY(reinterpret_cast<size_t>(childStorage) < reinterpret_cast<size_t>(parentStorage));
    Probe *parent = new (parentStorage) Probe(0, &parentAlive);
    LOKA_VERIFY(partition.registerNode(parent, 0));
    Probe *child = new (childStorage) Probe(parent, &childAlive);
    LOKA_VERIFY(partition.registerNode(child, parent));
    parent->addChild(child);
    Probe *orphan = new (orphanStorage) Probe(parent, &orphanAlive);
    LOKA_VERIFY(partition.registerNode(orphan, parent));
    child->addChild(new Probe(child));
    void *heapStorage = loka::core::LokaAllocRaw(sizeof(Probe), NodeHeapAllocationSite());
    LOKA_VERIFY(heapStorage);
    Probe *gateChild = new (heapStorage) Probe(child);
    gateChild->setGateAllocated(true);
    child->addChild(gateChild);
    LOKA_VERIFY(partition.registerHeap(new Probe(orphan), orphan));
    LOKA_VERIFY(!partition.destroy(childStorage, layout));
    LOKA_VERIFY(parentAlive && childAlive && orphanAlive && deaths == 0);
  }
  LOKA_VERIFY(!parentAlive && !childAlive && !orphanAlive);
  LOKA_VERIFY(deaths == 6 && frees == 2);
  // Reusing heap row zero must not make it a root ahead of its row-one owner.
  {
    NodePartition partition;
    LOKA_VERIFY(partition.boot(&layout, 1, 2));
    void *p = partition.allocate(layout);
    Probe *temporary = new (p) Probe();
    LOKA_VERIFY(partition.registerNode(temporary, 0));
    LOKA_VERIFY(partition.registerHeap(new Probe(), temporary));
    Probe *parent = new Probe(0, &parentAlive);
    LOKA_VERIFY(partition.registerHeap(parent, 0));
    LOKA_VERIFY(partition.destroy(p, layout));
    Probe *child = new Probe(parent);
    LOKA_VERIFY(partition.registerHeap(child, parent));
    parent->addChild(child);
  }
  LOKA_VERIFY(!parentAlive && deaths == 10 && frees == 3);
}

void testNodePartitionAlignmentOverflow()
{
  Backend backend;
  NodeSlotLayout layouts[] = {NodeSlotLayout(3, 64, 3), NodeSlotLayout(1, 1, 2)};
  size_t bytes = 0, stride = 0;
  LOKA_VERIFY(layouts[0].stride(stride) && stride == 64);
  LOKA_VERIFY(layouts[1].stride(stride) && stride >= sizeof(void *));
  NodePartition partition;
  LOKA_VERIFY(partition.boot(layouts, 2));
  for (int i = 0; i < 3; ++i)
  {
    void *p = partition.allocate(layouts[0]);
    LOKA_VERIFY(p && reinterpret_cast<size_t>(p) % 64 == 0);
  }
  LOKA_VERIFY(!partition.allocate(layouts[0]));
  NodeSlotLayout overflow[] = {NodeSlotLayout(size_t(-1), 64, 1),
                               NodeSlotLayout(64, 64, size_t(-1)),
                               NodeSlotLayout(0, 8, 1),
                               NodeSlotLayout(8, 0, 1),
                               NodeSlotLayout(8, 17, 1)};
  for (size_t i = 0; i < sizeof(overflow) / sizeof(overflow[0]); ++i)
    LOKA_VERIFY(!NodePartition::reservationBytes(overflow + i, 1, 0, bytes));
  LOKA_VERIFY(!NodePartition::reservationBytes(layouts, 2, size_t(-1), bytes));
  NodeSlotLayout sum[] = {NodeSlotLayout(size_t(-1) / 2, 1, 1), NodeSlotLayout(size_t(-1) / 2 - 1, 1, 1)};
  LOKA_VERIFY(!NodePartition::reservationBytes(sum, 2, 0, bytes));
  LOKA_VERIFY(acquisitions == 1);
  std::printf("NodePartition sizeof=%lu; layout=%lu; resident pair=%lu; occupancy(194/five classes)=26\n",
              static_cast<unsigned long>(sizeof(NodePartition)),
              static_cast<unsigned long>(sizeof(NodeSlotLayout)),
              static_cast<unsigned long>(2 * sizeof(Node *)));
}

void testNodePartitionBootRefusal()
{
  Backend backend;
  const NodeSlotLayout layout = NodeSlotLayout::of<Probe>(1);
  NodePartition partition;
  refuse = true;
  LOKA_VERIFY(!partition.boot(&layout, 1));
  LOKA_VERIFY(!partition.allocate(layout));
  refuse = false;
  LOKA_VERIFY(partition.boot(&layout, 1));
  LOKA_VERIFY(!partition.boot(&layout, 1));
  LOKA_VERIFY(acquisitions == 2);
  void *p = partition.allocate(layout);
  LOKA_VERIFY(p && partition.cancel(p, layout));
}
