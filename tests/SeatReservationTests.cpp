#include "SeatReservationTests.hpp"
#include "support/TestVerify.hpp"
#include "app/scene/boundary/detail/NodeBuildTicket.hpp"
#include "../example/MineSweeper/src/MainNode.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#ifdef LOKA_UPSTREAM_GAUGE_PIN
#include "support/UpstreamGaugePin.hpp"
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif

using namespace loka::app;
using namespace loka::app::scene;
using namespace loka::app::scene::detail;
namespace
{
  size_t countLayout(const SeatLayoutTable &table, const NodeSlotLayout &layout)
  {
    for (size_t i = 0; i < table.count(); ++i)
      if (table.layouts()[i].size() == layout.size() && table.layouts()[i].alignment() == layout.alignment())
        return table.layouts()[i].count();
    return 0;
  }
  struct Item : Node
  {
  };
  struct Substitute : Node
  {
  };
  struct Unknown : Node
  {
    char padding[97];
  };
  struct Factory
  {
    Factory()
        : calls(0),
          decline(false)
    {
    }
    Item *construct(void *storage)
    {
      ++this->calls;
      return this->decline ? 0 : new (storage) Item();
    }
    int calls;
    bool decline;
  };
  struct SubstituteFactory
  {
    Substitute *construct(void *storage)
    {
      return new (storage) Substitute();
    }
  };
  struct UnknownFactory
  {
    UnknownFactory()
        : calls(0)
    {
    }
    Unknown *construct(void *storage)
    {
      ++this->calls;
      return new (storage) Unknown();
    }
    int calls;
  };
  struct Build : NodeBuildOperation
  {
    Build()
        : root(0),
          child(0),
          unknown(false)
    {
    }
    bool buildAndAttach(NodeBuildTicket &ticket)
    {
      this->root = ticket.create<Item>(this->factory);
      LOKA_VERIFY(this->root);
      return this->attach(ticket);
    }
    bool attach(NodeBuildTicket &ticket)
    {
      if (this->unknown)
      {
        UnknownFactory other;
        Unknown *node = ticket.create<Unknown>(other, this->root);
#ifdef NDEBUG
        LOKA_VERIFY(!node);
        LOKA_VERIFY(other.calls == 0);
#else
        (void)node;
#endif
      }
      else
      {
        this->child = ticket.create<Item>(this->factory, this->root);
#ifdef NDEBUG
        LOKA_VERIFY(!this->child);
        LOKA_VERIFY(this->factory.calls == 1);
#endif
      }
      return true;
    }
    Factory factory;
    Item *root;
    Item *child;
    bool unknown;
  };
  void quotaCase(bool unknown)
  {
    NodePartition partition;
    const NodeSlotLayout capacity = NodeSlotLayout::of<Item>(9);
    const NodeSlotLayout demand = NodeSlotLayout::of<Item>(1);
    LOKA_VERIFY(partition.boot(&capacity, 1));
    Build build;
    build.unknown = unknown;
    LOKA_VERIFY(partition.buildFixture(&demand, 1, build));
    LOKA_VERIFY(partition.destroy(build.root, demand));
  }
  void expectQuotaRefusal(bool unknown)
  {
#ifdef NDEBUG
    quotaCase(unknown);
#elif defined(__linux__)
    const pid_t child = fork();
    LOKA_VERIFY(child >= 0);
    if (child == 0)
    {
      quotaCase(unknown);
      _exit(0);
    }
    int status = 0;
    LOKA_VERIFY(waitpid(child, &status, 0) == child);
    LOKA_VERIFY(WIFSIGNALED(status));
    LOKA_VERIFY(WTERMSIG(status) == SIGABRT);
#else
    // Debug builds without fork (macOS CI): the abort case is not executed
    // here; keep the function referenced so -Wunused-function stays clean.
    (void)unknown;
    (void)&quotaCase;
#endif
  }
  bool refuseBacking = false;
  int backingAllocations = 0, backingFrees = 0;
  int tableAllocations = 0, tableFrees = 0;
  size_t tableBytes = 0;
  void *tableStorage = 0;
  void *allocateBacking(size_t bytes, const loka::core::LokaAllocationSite &site)
  {
    if (std::strcmp(site.ownerTag, "NodePartition") == 0)
    {
      ++backingAllocations;
      if (refuseBacking)
        return 0;
    }
#ifdef LOKA_UPSTREAM_GAUGE_PIN
    const loka::core::UpstreamGauge before = upstreamPinSnapshot();
#endif
    void *storage = new (std::nothrow) char[bytes];
    if (std::strcmp(site.ownerTag, "SeatReservation") == 0)
    {
      ++tableAllocations;
      tableBytes += bytes;
      tableStorage = storage;
#ifdef LOKA_UPSTREAM_GAUGE_PIN
      const loka::core::UpstreamGauge after = upstreamPinSnapshot();
      upstreamPinCheck("SeatReservation table", before, after, 1, sizeof(SeatReservation));
      LOKA_VERIFY(after.attempts - before.attempts == 1);
      LOKA_VERIFY(after.bytesAcquired - before.bytesAcquired == sizeof(SeatReservation));
#endif
    }
    return storage;
  }
  void freeBacking(void *p, const loka::core::LokaAllocationSite &site)
  {
    if (std::strcmp(site.ownerTag, "NodePartition") == 0)
      ++backingFrees;
    if (std::strcmp(site.ownerTag, "SeatReservation") == 0)
      ++tableFrees;
    delete[] static_cast<char *>(p);
  }
} // namespace

void testSeatReservationBoardCensus()
{
  SeatLayoutTable table;
  typedef KeyedNodeRecipe<minesweeper::BoardNodeList>::Type Recipe;
  reservation::detail::validate<Recipe>();
  LOKA_VERIFY(reservation::detail::Emitter<Recipe>::emit(table));
  LOKA_VERIFY(table.count() == 6);
  const NodeSlotLayout expected[] = {NodeSlotLayout::of<KeyedGenerationRoot>(1),
                                     NodeSlotLayout::of<FragmentNode>(1),
                                     NodeSlotLayout::of<GridNode>(1),
                                     NodeSlotLayout::of<BoundarySectionNode>(64),
                                     NodeSlotLayout::of<minesweeper::MineCellNode>(64),
                                     NodeSlotLayout::of<CellNode>(64)};
  LOKA_VERIFY(table.normalize());
  size_t total = 0;
  for (size_t i = 0; i < table.count(); ++i)
    total += table.layouts()[i].count();
  LOKA_VERIFY(total == 195);
  for (size_t i = 0; i < 6; ++i)
  {
    size_t count = 0;
    for (size_t j = 0; j < 6; ++j)
      if (expected[i].size() == expected[j].size() && expected[i].alignment() == expected[j].alignment())
        count += expected[j].count();
    LOKA_VERIFY(countLayout(table, expected[i]) == count);
  }
  SeatReservations owner;
  const SeatReservation *installed = owner.install(table);
  LOKA_VERIFY(installed);
  size_t bytes = 0, expectedBytes = 0;
  LOKA_VERIFY(NodePartition::reservationBytes(table.layouts(), table.count(), 0, expectedBytes));
  LOKA_VERIFY(installed->reservationBytes(bytes));
  LOKA_VERIFY(bytes == expectedBytes);
  NodePartition fixturePartition;
  LOKA_VERIFY(fixturePartition.boot(table.layouts(), table.count()));
  std::printf("SeatReservation: nodes=%lu classes=%lu bank=%lu table=%lu owner=%lu ticket=%lu\n",
              static_cast<unsigned long>(total),
              static_cast<unsigned long>(table.count()),
              static_cast<unsigned long>(bytes),
              static_cast<unsigned long>(sizeof(SeatLayoutTable)),
              static_cast<unsigned long>(sizeof(SeatReservation)),
              static_cast<unsigned long>(sizeof(NodeBuildTicket)));
  for (size_t i = 0; i < table.count(); ++i)
    std::printf("layout size=%lu align=%lu count=%lu\n",
                static_cast<unsigned long>(table.layouts()[i].size()),
                static_cast<unsigned long>(table.layouts()[i].alignment()),
                static_cast<unsigned long>(table.layouts()[i].count()));
}

void testSeatReservationCheckedNormalization()
{
  SeatLayoutTable table;
  LOKA_VERIFY(table.append(NodeSlotLayout::of<Item>(4)));
  LOKA_VERIFY(table.append(NodeSlotLayout::of<Substitute>(5)));
  LOKA_VERIFY(table.normalize());
  LOKA_VERIFY(table.count() == 1);
  LOKA_VERIFY(table.layouts()[0].count() == 9);
  SeatLayoutTable overflow;
  LOKA_VERIFY(overflow.append(NodeSlotLayout::of<Item>(size_t(-1))));
  LOKA_VERIFY(overflow.append(NodeSlotLayout::of<Item>(1)));
  LOKA_VERIFY(!overflow.normalize());
  SeatLayoutTable bounded;
  for (int i = 0; i < SeatLayoutTable::capacity; ++i)
    LOKA_VERIFY(bounded.append(NodeSlotLayout::of<Item>(1)));
  LOKA_VERIFY(!bounded.append(NodeSlotLayout::of<Item>(1)));
}

void testSeatReservationOwnerRetry()
{
  backingAllocations = backingFrees = tableAllocations = tableFrees = 0;
  tableBytes = 0;
  tableStorage = 0;
  loka::core::LokaAllocSetBackend(allocateBacking, freeBacking);
  {
    SeatReservations owner;
    const SeatReservation *installed = 0;
    {
      SeatLayoutTable temporary;
      LOKA_VERIFY(temporary.append(NodeSlotLayout::of<Item>(9)));
      SeatLayoutTable invalid;
      LOKA_VERIFY(!owner.install(invalid));
      LOKA_VERIFY(tableAllocations == 0);
      refuseBacking = true;
      installed = owner.install(temporary);
      LOKA_VERIFY(installed);
    }
    LOKA_VERIFY(installed->layoutTable().count() == 1);
    LOKA_VERIFY(installed->layoutTable().layouts()[0].count() == 9);
    size_t bytes = 0;
    LOKA_VERIFY(installed->reservationBytes(bytes));
    LOKA_VERIFY(bytes > 9 * sizeof(Item));
    LOKA_VERIFY(backingAllocations == 0);
    LOKA_VERIFY(tableAllocations == 1);
    LOKA_VERIFY(backingFrees == 0);
  }
  LOKA_VERIFY(backingFrees == 0);
  LOKA_VERIFY(tableFrees == 1);
  refuseBacking = false;
  loka::core::LokaAllocSetBackend(0, 0);
}

void testSeatReservationQuotaThroughAttach()
{
  expectQuotaRefusal(false);
}
void testSeatReservationUnknownClass()
{
  expectQuotaRefusal(true);
}

void testSeatReservationCancelUnconstructed()
{
  struct CancelBuild : NodeBuildOperation
  {
    bool buildAndAttach(NodeBuildTicket &ticket)
    {
      Factory factory;
      factory.decline = true;
      LOKA_VERIFY(!ticket.create<Item>(factory));
      LOKA_VERIFY(factory.calls == 1);
      return true;
    }
  } build;
  NodePartition partition;
  const NodeSlotLayout layout = NodeSlotLayout::of<Item>(1);
  LOKA_VERIFY(partition.boot(&layout, 1));
  LOKA_VERIFY(partition.buildFixture(&layout, 1, build));
  void *slot = partition.allocate(layout);
  LOKA_VERIFY(slot);
  LOKA_VERIFY(partition.cancel(slot, layout));
  LOKA_VERIFY(!partition.cancel(slot, layout));
}

void testSeatReservationAggregateSubstitution()
{
  struct AggregateBuild : NodeBuildOperation
  {
    AggregateBuild()
        : node(0)
    {
    }
    bool buildAndAttach(NodeBuildTicket &ticket)
    {
      SubstituteFactory factory;
      this->node = ticket.create<Substitute>(factory);
      return this->node != 0;
    }
    Substitute *node;
  } build;
  LOKA_VERIFY(sizeof(Item) == sizeof(Substitute));
  NodePartition partition;
  const NodeSlotLayout layout = NodeSlotLayout::of<Item>(1);
  LOKA_VERIFY(partition.boot(&layout, 1));
  LOKA_VERIFY(partition.buildFixture(&layout, 1, build));
  LOKA_VERIFY(partition.destroy(build.node, layout));
}

namespace
{
  struct ColdOwner : BoundaryNode
  {
    virtual void composeWithContext(ComponentContext &, ComposeEvent) {}
    void declare(NodeComposition &composition)
    {
      composition.declare(Fragment());
    }
  };
} // namespace

void testSeatReservationKeyedColdInstall()
{
  backingAllocations = backingFrees = tableAllocations = tableFrees = 0;
  tableBytes = 0;
  tableStorage = 0;
  loka::core::LokaAllocSetBackend(allocateBacking, freeBacking);
  {
    ColdOwner owner;
    loka::core::MutableState<int> key(0);
    typedef reservation::SeatNodes<reservation::Nodes<FragmentNode, 1> > Descriptor;
    KeyedDefinition<int> seat(key, &owner, &ColdOwner::declare, Descriptor());
    ComponentContext context;
    context.setBoundary(&owner);
    context.setOwner(&owner);
    context.setStateOwner(&owner);
    refuseBacking = true;

    {
      loka::core::OwnedDef<BranchSeatDeclaration> candidate(seat.declareBranchCandidate(context));
      const bool created = candidate.isSet();
      LOKA_VERIFY(created);
    }
    LOKA_VERIFY(backingAllocations == 0);
    LOKA_VERIFY(tableAllocations == 1);
    {
      loka::core::OwnedDef<BranchSeatDeclaration> candidate(seat.declareBranchCandidate(context));
      const bool created = candidate.isSet();
      LOKA_VERIFY(created);
    }
    LOKA_VERIFY(backingAllocations == 0);
    LOKA_VERIFY(tableAllocations == 1);
    {
      loka::core::OwnedDef<NodeDefinitionBase> clone(seat.clone());
      LOKA_VERIFY(clone.get());
      loka::core::OwnedDef<BranchSeatDeclaration> candidate(
          clone->asBranchSeatDefinition()->declareBranchCandidate(context));
      const bool created = candidate.isSet();
      LOKA_VERIFY(created);
    }
    LOKA_VERIFY(backingAllocations == 0);
    LOKA_VERIFY(tableAllocations == 2);
    LOKA_VERIFY(backingFrees == 0);
  }
  LOKA_VERIFY(backingFrees == 0);
  LOKA_VERIFY(tableFrees == 2);
  refuseBacking = false;
  loka::core::LokaAllocSetBackend(0, 0);
}

namespace
{
  size_t countMineCells(Node *node)
  {
    if (!node)
      return 0;
    size_t count = node->asCellNode() != 0 ? 1 : 0;
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
      count += countMineCells(child);
    return count;
  }
} // namespace

void testSeatReservationMineSweeperNoBacking()
{
  backingAllocations = backingFrees = tableAllocations = tableFrees = 0;
  tableBytes = 0;
  tableStorage = 0;
  refuseBacking = true;
  loka::core::LokaAllocSetBackend(allocateBacking, freeBacking);
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<minesweeper::MainNode>(minesweeper::MainProps(12345))));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    LOKA_VERIFY(countMineCells(loka::dsl::testing::SceneTestAccess::rootNode(scene)) == 64);
    LOKA_VERIFY(backingAllocations == 0);
    LOKA_VERIFY(tableAllocations == 1);
    LOKA_VERIFY(tableBytes == sizeof(SeatReservation));
    LOKA_VERIFY(tableStorage);
    const SeatReservation &installed = *static_cast<const SeatReservation *>(tableStorage);
    LOKA_VERIFY(installed.layoutTable().count() == 6);
    size_t slots = 0;
    for (size_t i = 0; i < installed.layoutTable().count(); ++i)
      slots += installed.layoutTable().layouts()[i].count();
    LOKA_VERIFY(slots == 195);
    size_t bytes = 0, fixtureBytes = 0;
    LOKA_VERIFY(installed.reservationBytes(bytes));
    SeatLayoutTable fixtureTable;
    typedef KeyedNodeRecipe<minesweeper::BoardNodeList>::Type Recipe;
    LOKA_VERIFY(reservation::detail::Emitter<Recipe>::emit(fixtureTable));
    LOKA_VERIFY(fixtureTable.normalize());
    LOKA_VERIFY(NodePartition::reservationBytes(fixtureTable.layouts(), fixtureTable.count(), 0, fixtureBytes));
    LOKA_VERIFY(bytes == fixtureBytes);
    std::printf("MineSweeper dormant census: table=%lu bytes/%d allocation; partition=%d; cells=64; slots=%lu; "
                "classes=6; planned=%lu\n",
                static_cast<unsigned long>(tableBytes),
                tableAllocations,
                backingAllocations,
                static_cast<unsigned long>(slots),
                static_cast<unsigned long>(bytes));
  }
  LOKA_VERIFY(backingFrees == 0);
  LOKA_VERIFY(tableFrees == 1);
  refuseBacking = false;
  loka::core::LokaAllocSetBackend(0, 0);
}
