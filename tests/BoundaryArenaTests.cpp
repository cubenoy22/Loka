#include "BoundaryArenaTests.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <new>
#include <vector>
#include "app/scene/boundary/detail/BoundaryArena.hpp"
#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/state/StateBatchBase.hpp"
#include "core/LokaAlloc.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include <cstring>
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/scene/Scene.hpp"
#include "support/RecomposingBoundary.hpp"
#include "support/RecordingPlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{

  // Counting gate backend: proves which acquisitions cross the gate while
  // still serving real storage. It is installed only around
  // allocation-balanced regions so storage always returns through the
  // backend that produced it.
  int g_gateBackendAllocCalls = 0;
  int g_gateBackendFreeCalls = 0;

  void *countingGateBackendAlloc(std::size_t size, const loka::core::LokaAllocationSite &site)
  {
    (void)site;
    ++g_gateBackendAllocCalls;
    return new (std::nothrow) char[size];
  }

  void countingGateBackendFree(void *ptr, const loka::core::LokaAllocationSite &site)
  {
    (void)site;
    ++g_gateBackendFreeCalls;
    delete[] static_cast<char *>(ptr);
  }

  static bool isPowerOfTwo(size_t value)
  {
    return value != 0 && (value & (value - 1)) == 0;
  }

  static bool isAligned(void *ptr, size_t align)
  {
    size_t address = reinterpret_cast<size_t>(ptr);
    return (address & (align - 1)) == 0;
  }

  struct StateArenaDestroyLog
  {
    int count;
    loka::core::StateBase *first;
    loka::core::StateBase *second;
  };

  static StateArenaDestroyLog g_stateArenaDestroyLog = {0, 0, 0};

  static void resetStateArenaDestroyLog()
  {
    g_stateArenaDestroyLog.count = 0;
    g_stateArenaDestroyLog.first = 0;
    g_stateArenaDestroyLog.second = 0;
  }

  static void recordStateDestroy(loka::core::StateBase *state)
  {
    ++g_stateArenaDestroyLog.count;
    if (!g_stateArenaDestroyLog.first)
    {
      g_stateArenaDestroyLog.first = state;
    }
    else if (!g_stateArenaDestroyLog.second)
    {
      g_stateArenaDestroyLog.second = state;
    }
  }

  class TestStateOwner : public loka::app::scene::IStateOwner
  {
  public:
    TestStateOwner()
        : tracker_(),
          ownedStates_(),
          arena_()
    {
    }

    ~TestStateOwner()
    {
      this->clearOwnedStates();
    }

    virtual void adoptState(loka::core::StateBase *state)
    {
      if (!state)
      {
        return;
      }
      ownedStates_.push_back(state);
      tracker_.addState(state);
    }

    virtual void adoptStateUnchecked(loka::core::StateBase *state)
    {
      if (!state)
      {
        return;
      }
      ownedStates_.push_back(state);
      tracker_.addStateUnchecked(state);
    }

    virtual void releaseState(loka::core::StateBase *state)
    {
      if (!state)
      {
        return;
      }
      for (size_t i = 0; i < ownedStates_.size();)
      {
        if (ownedStates_[i] == state)
        {
          ownedStates_.erase(ownedStates_.begin() + i);
        }
        else
        {
          ++i;
        }
      }
      tracker_.removeState(state);
      if (state->isArenaAllocated())
      {
        arena_.releaseState(state);
      }
      else
      {
        loka::app::scene::DestroyAdoptedHeapState(state);
      }
    }

    virtual void reserveStates(size_t count)
    {
      ownedStates_.reserve(ownedStates_.size() + count);
      tracker_.reserveStates(count);
    }

    virtual void reserveStateArena(size_t totalSize)
    {
      arena_.reserve(totalSize);
    }

    virtual void *allocateStateMemory(size_t size, size_t align)
    {
      return arena_.allocate(size, align);
    }

    virtual void registerStateMemory(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
    {
      arena_.registerState(state, destroy);
    }

    virtual loka::core::StateTracker *tracker()
    {
      return &tracker_;
    }

  private:
    void clearOwnedStates()
    {
      for (size_t i = 0; i < ownedStates_.size(); ++i)
      {
        loka::core::StateBase *state = ownedStates_[i];
        if (!state)
        {
          continue;
        }
        tracker_.removeState(state);
        if (!state->isArenaAllocated())
        {
          loka::app::scene::DestroyAdoptedHeapState(state);
        }
      }
      ownedStates_.clear();
      arena_.clear();
    }

    loka::core::PushStateTracker tracker_;
    std::vector<loka::core::StateBase *> ownedStates_;
    loka::app::scene::StateArena arena_;
  };

  static void testNodeArenaAlignmentAndCapacity()
  {
    const size_t minAlign = loka::app::scene::detail::NormalizeArenaAlign(1);
    assert(minAlign >= sizeof(void *));
    assert(isPowerOfTwo(minAlign));

    const size_t roundedAlign = loka::app::scene::detail::NormalizeArenaAlign(3);
    assert(roundedAlign >= 3);
    assert(isPowerOfTwo(roundedAlign));

    loka::app::scene::NodeArena arena;
    assert(!arena.hasCapacity());
    assert(arena.allocate(4, 4) == 0);

    arena.reserve(32);
    assert(arena.hasCapacity());

    void *first = arena.allocate(4, 4);
    assert(first != 0);
    assert(isAligned(first, loka::app::scene::detail::NormalizeArenaAlign(4)));

    void *second = arena.allocate(8, 8);
    assert(second != 0);
    assert(isAligned(second, loka::app::scene::detail::NormalizeArenaAlign(8)));
    assert(second != first);

    assert(arena.allocate(1024, 4) == 0);

    arena.reserve(16);
    assert(arena.hasCapacity());
    void *afterReserve = arena.allocate(16, 4);
    assert(afterReserve != 0);

    arena.clear();
    assert(!arena.hasCapacity());
    assert(arena.allocate(4, 4) == 0);
  }

  static void testNodeArenaRegistrationStampsOwner()
  {
    loka::app::scene::Node *heapNode = new loka::app::scene::Node();
    assert(heapNode->arenaOwner() == 0);
    assert(!heapNode->isArenaAllocated());
    delete heapNode;

    loka::app::scene::NodeArena arena;
    const size_t nodeBytes =
        sizeof(loka::app::scene::Node) + loka::app::scene::detail::AlignOf<loka::app::scene::Node>::value;
    arena.reserve(nodeBytes);
    void *nodeMemory = arena.allocate(sizeof(loka::app::scene::Node),
                                      loka::app::scene::detail::AlignOf<loka::app::scene::Node>::value);
    assert(nodeMemory != 0);
    loka::app::scene::Node *arenaNode = new (nodeMemory) loka::app::scene::Node();
    assert(arenaNode->arenaOwner() == 0);
    assert(!arenaNode->isArenaAllocated());

    arena.registerNode(arenaNode);
    assert(arenaNode->arenaOwner() == &arena);
    assert(arenaNode->isArenaAllocated());
  }

  class NodeArenaDestroyOrderProbe : public loka::app::scene::Node
  {
  public:
    NodeArenaDestroyOrderProbe(std::vector<int> *destroyOrder, int marker)
        : destroyOrder_(destroyOrder),
          marker_(marker)
    {
    }

    virtual ~NodeArenaDestroyOrderProbe()
    {
      this->destroyOrder_->push_back(this->marker_);
    }

  private:
    std::vector<int> *destroyOrder_;
    int marker_;
  };

  class LinkedNodeArenaDestroyOrderProbe : public loka::app::scene::NestableNode
  {
  public:
    LinkedNodeArenaDestroyOrderProbe(std::vector<int> *destroyOrder, int marker)
        : destroyOrder_(destroyOrder),
          marker_(marker)
    {
    }

    virtual ~LinkedNodeArenaDestroyOrderProbe()
    {
      this->destroyOrder_->push_back(this->marker_);
    }

  private:
    std::vector<int> *destroyOrder_;
    int marker_;
  };

  class NodeArenaDestroyCountProbe : public loka::app::scene::Node
  {
  public:
    explicit NodeArenaDestroyCountProbe(int *destructorCalls)
        : destructorCalls_(destructorCalls)
    {
    }

    virtual ~NodeArenaDestroyCountProbe()
    {
      ++*this->destructorCalls_;
    }

  private:
    int *destructorCalls_;
  };

  static void testNodeArenaDestroysChildrenBeforeParents()
  {
    const size_t probeBytes = sizeof(NodeArenaDestroyOrderProbe)
                              + loka::app::scene::detail::AlignOf<NodeArenaDestroyOrderProbe>::value;
    std::vector<int> destroyOrder;
    loka::app::scene::NodeArena arena;
    arena.reserve(probeBytes * 2);

    void *parentMemory = arena.allocate(
        sizeof(NodeArenaDestroyOrderProbe),
        loka::app::scene::detail::AlignOf<NodeArenaDestroyOrderProbe>::value);
    assert(parentMemory != 0);
    NodeArenaDestroyOrderProbe *parent = new (parentMemory) NodeArenaDestroyOrderProbe(&destroyOrder, 1);
    arena.registerNode(parent);

    void *childMemory = arena.allocate(
        sizeof(NodeArenaDestroyOrderProbe),
        loka::app::scene::detail::AlignOf<NodeArenaDestroyOrderProbe>::value);
    assert(childMemory != 0);
    NodeArenaDestroyOrderProbe *child = new (childMemory) NodeArenaDestroyOrderProbe(&destroyOrder, 2);
    arena.registerNode(child);

    arena.clear();

    assert(destroyOrder.size() == 2);
    assert(destroyOrder[0] == 2 &&
           "NodeArena::clear must destroy children before parents");
    assert(destroyOrder[1] == 1);

    const size_t linkedParentBytes = sizeof(LinkedNodeArenaDestroyOrderProbe)
                                     + loka::app::scene::detail::AlignOf<LinkedNodeArenaDestroyOrderProbe>::value;
    destroyOrder.clear();
    arena.reserve(linkedParentBytes + probeBytes);

    void *linkedParentMemory = arena.allocate(
        sizeof(LinkedNodeArenaDestroyOrderProbe),
        loka::app::scene::detail::AlignOf<LinkedNodeArenaDestroyOrderProbe>::value);
    assert(linkedParentMemory != 0);
    LinkedNodeArenaDestroyOrderProbe *linkedParent =
        new (linkedParentMemory) LinkedNodeArenaDestroyOrderProbe(&destroyOrder, 1);
    arena.registerNode(linkedParent);

    void *linkedChildMemory = arena.allocate(
        sizeof(NodeArenaDestroyOrderProbe),
        loka::app::scene::detail::AlignOf<NodeArenaDestroyOrderProbe>::value);
    assert(linkedChildMemory != 0);
    NodeArenaDestroyOrderProbe *linkedChild =
        new (linkedChildMemory) NodeArenaDestroyOrderProbe(&destroyOrder, 2);
    arena.registerNode(linkedChild);
    linkedParent->addChild(linkedChild);

    arena.clear();

    assert(destroyOrder.size() == 2);
    assert(destroyOrder[0] == 2 &&
           "NodeArena::clear must sever linked parents before reverse destruction");
    assert(destroyOrder[1] == 1);
  }

  static void testNodeArenaReleaseTombstonePreventsClearDoubleDestruction()
  {
    const size_t probeBytes = sizeof(NodeArenaDestroyCountProbe)
                              + loka::app::scene::detail::AlignOf<NodeArenaDestroyCountProbe>::value;
    int firstDestructorCalls = 0;
    int secondDestructorCalls = 0;

    {
      loka::app::scene::NodeArena arena;
      arena.reserve(probeBytes * 2);

      void *firstMemory = arena.allocate(
          sizeof(NodeArenaDestroyCountProbe),
          loka::app::scene::detail::AlignOf<NodeArenaDestroyCountProbe>::value);
      assert(firstMemory != 0);
      NodeArenaDestroyCountProbe *first =
          new (firstMemory) NodeArenaDestroyCountProbe(&firstDestructorCalls);
      arena.registerNode(first);

      void *secondMemory = arena.allocate(
          sizeof(NodeArenaDestroyCountProbe),
          loka::app::scene::detail::AlignOf<NodeArenaDestroyCountProbe>::value);
      assert(secondMemory != 0);
      NodeArenaDestroyCountProbe *second =
          new (secondMemory) NodeArenaDestroyCountProbe(&secondDestructorCalls);
      arena.registerNode(second);

      assert(arena.releaseNode(first));
      assert(firstDestructorCalls == 1);
      assert(secondDestructorCalls == 0);

      arena.clear();
      assert(firstDestructorCalls == 1 &&
             "NodeArena::clear must skip a released node's tombstoned ledger entry");
      assert(secondDestructorCalls == 1);
    }

    assert(firstDestructorCalls == 1);
    assert(secondDestructorCalls == 1);
  }

  static void testNodeArenaClearDestroysHeapChildOfArenaParentExactlyOnce()
  {
    const size_t parentBytes = sizeof(LinkedNodeArenaDestroyOrderProbe)
                               + loka::app::scene::detail::AlignOf<LinkedNodeArenaDestroyOrderProbe>::value;
    std::vector<int> parentDestroyOrder;
    int heapChildDestructorCalls = 0;

    {
      loka::app::scene::NodeArena arena;
      arena.reserve(parentBytes);

      void *parentMemory = arena.allocate(
          sizeof(LinkedNodeArenaDestroyOrderProbe),
          loka::app::scene::detail::AlignOf<LinkedNodeArenaDestroyOrderProbe>::value);
      assert(parentMemory != 0);
      LinkedNodeArenaDestroyOrderProbe *parent =
          new (parentMemory) LinkedNodeArenaDestroyOrderProbe(&parentDestroyOrder, 1);
      arena.registerNode(parent);

      NodeArenaDestroyCountProbe *heapChild =
          new NodeArenaDestroyCountProbe(&heapChildDestructorCalls);
      assert(!heapChild->isArenaAllocated());
      parent->addChild(heapChild);

      arena.clear();
      assert(heapChildDestructorCalls == 1 &&
             "NodeArena::clear must delete a heap child detached from an arena parent exactly once");
      assert(parentDestroyOrder.size() == 1);
      assert(parentDestroyOrder[0] == 1);

      arena.clear();
      assert(heapChildDestructorCalls == 1);
    }

    assert(heapChildDestructorCalls == 1);
    assert(parentDestroyOrder.size() == 1);
  }

  static void testNodeArenaDetachAndDestroyRetiredGeneration()
  {
    const size_t tombstoneBytes = sizeof(NodeArenaDestroyCountProbe)
                                  + loka::app::scene::detail::AlignOf<NodeArenaDestroyCountProbe>::value;
    const size_t parentBytes = sizeof(LinkedNodeArenaDestroyOrderProbe)
                               + loka::app::scene::detail::AlignOf<LinkedNodeArenaDestroyOrderProbe>::value;
    int tombstoneDestructorCalls = 0;
    int heapChildDestructorCalls = 0;
    std::vector<int> parentDestroyOrder;
    loka::app::scene::NodeArena arena;
    arena.reserve(tombstoneBytes + parentBytes);

    void *tombstoneMemory = arena.allocate(
        sizeof(NodeArenaDestroyCountProbe),
        loka::app::scene::detail::AlignOf<NodeArenaDestroyCountProbe>::value);
    assert(tombstoneMemory != 0);
    NodeArenaDestroyCountProbe *tombstone =
        new (tombstoneMemory) NodeArenaDestroyCountProbe(&tombstoneDestructorCalls);
    arena.registerNode(tombstone);

    void *parentMemory = arena.allocate(
        sizeof(LinkedNodeArenaDestroyOrderProbe),
        loka::app::scene::detail::AlignOf<LinkedNodeArenaDestroyOrderProbe>::value);
    assert(parentMemory != 0);
    LinkedNodeArenaDestroyOrderProbe *parent =
        new (parentMemory) LinkedNodeArenaDestroyOrderProbe(&parentDestroyOrder, 1);
    arena.registerNode(parent);
    parent->addChild(new NodeArenaDestroyCountProbe(&heapChildDestructorCalls));

    assert(arena.releaseNode(tombstone));
    assert(tombstoneDestructorCalls == 1);

    loka::app::scene::NodeArena::RetiredNodeGeneration generation;
    assert(arena.detachRetiredGeneration(generation));
    assert(!arena.hasCapacity());
    assert(arena.allocate(4, 4) == 0);
    assert(generation.nodes.size() == 2);
    assert(generation.nodes[0] == 0 &&
           "detaching a generation must preserve arena ledger tombstones");

    arena.reserve(32);
    assert(arena.hasCapacity());
    assert(arena.allocate(4, 4) != 0 &&
           "an emptied arena must accept a fresh reservation while its old generation is retired");

    loka::app::scene::NodeArena::destroyRetiredGeneration(generation);
    assert(tombstoneDestructorCalls == 1 &&
           "retired generation destruction must skip tombstoned ledger entries");
    assert(heapChildDestructorCalls == 1 &&
           "retired generation destruction must delete detached heap children exactly once");
    assert(parentDestroyOrder.size() == 1 && parentDestroyOrder[0] == 1);

    loka::app::scene::NodeArena::destroyRetiredGeneration(generation);
    assert(tombstoneDestructorCalls == 1);
    assert(heapChildDestructorCalls == 1);
    assert(parentDestroyOrder.size() == 1);
  }

  static void testStateArenaAllocation()
  {
    loka::app::scene::StateArena arena;
    assert(!arena.hasBlocks());
    assert(arena.allocate(4, 4) == 0);

    arena.reserve(32);
    assert(arena.hasBlocks());

    void *first = arena.allocate(4, 4);
    assert(first != 0);
    assert(isAligned(first, loka::app::scene::detail::NormalizeArenaAlign(4)));

    void *second = arena.allocate(8, 8);
    assert(second != 0);
    assert(isAligned(second, loka::app::scene::detail::NormalizeArenaAlign(8)));
    assert(second != first);

    // Allocation may consume reserved capacity, but only reserve() may grow the
    // owner-lifetime arena. Unreserved state can then fall back to the heap.
    assert(arena.allocate(1024, 4) == 0);

    arena.reserve(1024);
    void *grown = arena.allocate(1024, 4);
    assert(grown != 0);
    assert(isAligned(grown, loka::app::scene::detail::NormalizeArenaAlign(4)));

    arena.clear();
    assert(!arena.hasBlocks());
    assert(arena.allocate(4, 4) == 0);
  }

  static void testStateArenaReleaseAndClear()
  {
    resetStateArenaDestroyLog();

    loka::core::MutableState<int> first(1);
    loka::core::MutableState<int> second(2);

    loka::app::scene::StateArena arena;
    arena.registerState(&first, &recordStateDestroy);
    arena.registerState(&second, &recordStateDestroy);

    arena.releaseState(&first);
    assert(g_stateArenaDestroyLog.count == 1);
    assert(g_stateArenaDestroyLog.first == &first);

    arena.releaseState(&first);
    assert(g_stateArenaDestroyLog.count == 1);

    arena.clear();
    assert(g_stateArenaDestroyLog.count == 2);
    assert(g_stateArenaDestroyLog.second == &second);

    arena.clear();
    assert(g_stateArenaDestroyLog.count == 2);
  }

  template <typename T> static void testStateArenaReserveCoversBatchEstimate(const T &sample)
  {
    (void)sample;
    typedef loka::core::MutableState<T> MutableStateType;

    const size_t count = 8;
    loka::app::scene::StateArena arena;
    arena.reserve(loka::app::scene::StateBatchBase::ArenaBytesForState<T>() * count);

    for (size_t i = 0; i < count; ++i)
    {
      void *mem = arena.allocate(sizeof(MutableStateType),
                                 loka::app::scene::detail::AlignOf<MutableStateType>::value);
      assert(mem != 0);
    }
  }

  static void testStateArenaReserveMatchesBatchEstimate()
  {
    testStateArenaReserveCoversBatchEstimate(char(0));
    testStateArenaReserveCoversBatchEstimate(double(0));
    testStateArenaReserveCoversBatchEstimate(loka::core::String::Literal("arena"));
  }

  static void testStateArenaGrowsAcrossOwnerBatches()
  {
    TestStateOwner owner;
    loka::app::scene::NodeState<int> firstBatch[4];
    loka::app::scene::NodeState<int> secondBatch[4];
    loka::app::scene::NodeState<int> immediate;

    {
      loka::app::scene::NodeComposition::StateBatch batch(&owner);
      for (int i = 0; i < 4; ++i)
      {
        batch.state(firstBatch[i], 10 + i);
      }
    }

    {
      loka::app::scene::NodeComposition::StateBatch batch(&owner);
      for (int i = 0; i < 4; ++i)
      {
        batch.state(secondBatch[i], 20 + i);
      }
    }

    loka::app::scene::StateBatchBase::CreateImmediateState<int>(&owner, immediate, 99);

    for (int i = 0; i < 4; ++i)
    {
      assert(firstBatch[i].isValid());
      assert(firstBatch[i].dangerouslyMutableState()->isArenaAllocated());
      assert(firstBatch[i].get() == 10 + i);
      assert(secondBatch[i].isValid());
      assert(secondBatch[i].dangerouslyMutableState()->isArenaAllocated());
      assert(secondBatch[i].get() == 20 + i);
    }

    assert(immediate.isValid());
    assert(immediate.dangerouslyMutableState()->isArenaAllocated());
    assert(immediate.get() == 99);
  }

  struct LargeStateValue
  {
    char bytes[128];

    bool operator!=(const LargeStateValue &other) const
    {
      for (size_t i = 0; i < sizeof(bytes); ++i)
      {
        if (bytes[i] != other.bytes[i])
        {
          return true;
        }
      }
      return false;
    }
  };

  static void testUnreservedStateFallsBackAfterReservedCapacityIsConsumed()
  {
    const loka::core::LokaAllocationSite heapStateSite("StateOwner", "MutableState");
#ifdef LOKA_LIFECYCLE_AUDIT
    const int heapLiveBefore = loka::core::LokaAllocAuditLiveCount(heapStateSite);
#endif
    g_gateBackendAllocCalls = 0;
    g_gateBackendFreeCalls = 0;
    loka::core::LokaAllocSetBackend(&countingGateBackendAlloc, &countingGateBackendFree);
    {
      TestStateOwner owner;
      loka::app::scene::NodeState<LargeStateValue> reserved;
      loka::app::scene::NodeState<LargeStateValue> unreserved;
      loka::app::scene::NodeState<LargeStateValue> immediate;
      LargeStateValue initial = {{0}};

      owner.reserveStateArena(
          loka::app::scene::StateBatchBase::ArenaBytesForState<LargeStateValue>());
      loka::app::scene::StateBatchBase::CreateStateFromInitial<LargeStateValue>(
          &owner, reserved, initial);
      loka::app::scene::StateBatchBase::CreateStateFromInitial<LargeStateValue>(
          &owner, unreserved, initial);

      assert(reserved.dangerouslyMutableState()->isArenaAllocated());
      assert(!unreserved.dangerouslyMutableState()->isArenaAllocated());

      // Red before #132 S2: the heap fallback was a plain throwing new that
      // bypassed the gate. Now it crosses the gate and carries provenance so
      // release returns the storage through the backend that produced it.
      assert(unreserved.dangerouslyMutableState()->isGateAllocated());
      assert(!reserved.dangerouslyMutableState()->isGateAllocated());
#ifdef LOKA_LIFECYCLE_AUDIT
      assert(loka::core::LokaAllocAuditLiveCount(heapStateSite) == heapLiveBefore + 1);
#endif

      loka::app::scene::StateBatchBase::CreateImmediateState<LargeStateValue>(
          &owner, immediate, initial);
      assert(immediate.dangerouslyMutableState()->isArenaAllocated());
    }
    // Slabs, block headers, and the heap-fallback state all returned through
    // the gate: the counting backend saw every acquisition come back.
    assert(g_gateBackendAllocCalls >= 3);
    assert(g_gateBackendFreeCalls == g_gateBackendAllocCalls);
    loka::core::LokaAllocSetBackend(0, 0);
#ifdef LOKA_LIFECYCLE_AUDIT
    assert(loka::core::LokaAllocAuditLiveCount(heapStateSite) == heapLiveBefore);
    loka::core::LokaAllocAuditCheckpoint(
        "testUnreservedStateFallsBackAfterReservedCapacityIsConsumed");
#endif
  }

  // A minimal concrete node/definition to exercise heap-node creation
  // through the allocation gate (#132 S2b). NodeDefinition::create() is the
  // one real heap-node alloc site; this probe reaches it directly.
  class GateProbeNode;
  struct GateProbeTypeTag
  {
  };

  struct GateProbeProps : public loka::app::scene::NodePropsBase<GateProbeProps>
  {
    typedef GateProbeTypeTag TypeTag;
    typedef GateProbeNode NodeType;
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false
                                                      : this->propsTypeId() < rhs.propsTypeId();
    }
  };

  class GateProbeNode : public loka::app::scene::Node
  {
  public:
    typedef GateProbeTypeTag TypeTag;
    GateProbeProps props;
    GateProbeNode(const GateProbeProps &p)
        : props(p)
    {
    }
  };

  struct GateProbeDefinition : public loka::app::scene::NodeDefinition<GateProbeProps, GateProbeNode>
  {
  };


  // White-flag backend fakes (#132 S3). The refusing backend surrenders every
  // acquisition, so a compose window under it exercises both storage doors:
  // the slab append is refused first, then the gate-routed heap fallback. The
  // selective backend refuses only the heap-fallback site so the arena path
  // stays real. Frees delegate to the default-compatible delete[] because the
  // window may release storage the default backend produced.
  int g_refusingBackendRefusals = 0;

  void *refusingBackendAlloc(std::size_t size, const loka::core::LokaAllocationSite &site)
  {
    (void)size;
    (void)site;
    ++g_refusingBackendRefusals;
    return 0;
  }

  int g_heapStateRefusals = 0;

  void *heapStateRefusingBackendAlloc(std::size_t size, const loka::core::LokaAllocationSite &site)
  {
    if (std::strcmp(site.ownerTag, "StateOwner") == 0)
    {
      ++g_heapStateRefusals;
      return 0;
    }
    return new (std::nothrow) char[size];
  }

  void delegatingBackendFree(void *ptr, const loka::core::LokaAllocationSite &site)
  {
    (void)site;
    delete[] static_cast<char *>(ptr);
  }

  // 32-byte payload: fits the StateBatch inline initializer storage while
  // keeping sizeof(MutableState<T>) well above the alignment slack a consumed
  // arena block can have left, so a redeclared batch always needs a new slab.
  struct WhiteFlagStatePayload
  {
    char bytes[32];

    bool operator!=(const WhiteFlagStatePayload &other) const
    {
      for (size_t i = 0; i < sizeof(bytes); ++i)
      {
        if (bytes[i] != other.bytes[i])
        {
          return true;
        }
      }
      return false;
    }
  };

  class WhiteFlagRecomposeRootNode;
  typedef loka::app::scene::BoundaryPropsFor<WhiteFlagRecomposeRootNode> WhiteFlagRecomposeRootProps;

  /** Scene-root boundary that redeclares its node-local states on every
      compose pass, so an externally driven recompose issues fresh gate
      acquisitions: first the arena slab append, then the heap fallback. */
  class WhiteFlagRecomposeRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<WhiteFlagRecomposeRootNode,
                                                         WhiteFlagRecomposeRootProps>
  {
  public:
    explicit WhiteFlagRecomposeRootNode(const WhiteFlagRecomposeRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<WhiteFlagRecomposeRootNode,
                                                    WhiteFlagRecomposeRootProps>(props),
          first_(),
          second_()
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      WhiteFlagStatePayload initial = {{0}};
      composition.declareStates().state(this->first_, initial).state(this->second_, initial);
      composition.declare(loka::app::FragmentDefinition());
    }

    loka::app::scene::NodeState<WhiteFlagStatePayload> first_;
    loka::app::scene::NodeState<WhiteFlagStatePayload> second_;
  };

  // Scene-root refusal harness (#132 ruling 3 / #140 P2). On this branch the
  // root create() is still a plain `new NodeT` and cannot return 0 under the
  // gate's refusing backend (S2b/#140 is not merged here), so the refusal is
  // injected through a test-only root definition whose create() returns 0 on
  // demand — standing in faithfully for the gate-routed root create() that
  // #140 will make return 0. isBoundary() stays true; clone() preserves the
  // refusal so the Scene's owned clone refuses too.
  bool g_refuseRootCreate = false;

  class RefusableRootNode;
  typedef loka::app::scene::BoundaryPropsFor<RefusableRootNode> RefusableRootProps;

  class RefusableRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<RefusableRootNode, RefusableRootProps>
  {
  public:
    explicit RefusableRootNode(const RefusableRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<RefusableRootNode, RefusableRootProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::FragmentDefinition());
    }
  };

  struct RefusableRootDefinition
      : public loka::app::scene::BoundaryDefinition<RefusableRootProps, RefusableRootNode>
  {
    typedef loka::app::scene::BoundaryDefinition<RefusableRootProps, RefusableRootNode> BaseType;
    RefusableRootDefinition() : BaseType() {}
    RefusableRootDefinition(const RefusableRootDefinition &other) : BaseType(other) {}
    virtual loka::app::scene::Node *create() const
    {
      if (g_refuseRootCreate)
      {
        return 0;
      }
      return BaseType::create();
    }
    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return new RefusableRootDefinition(*this);
    }
  };

  // Local-rebuild node-refusal harness (#132 ruling 3, Codex P2 hole 1). A
  // contextless local rebuild materializes a fresh child node through a
  // temporary NodeComposition with no ComponentContext, so the boundary is
  // unreachable inside createNodeFromDefinition. This backend refuses only the
  // gate-routed heap node site so the initial mount and every other allocation
  // succeed; the refusal is armed just around the child-introducing recompose.
  bool g_localRebuildRefuseNode = false;
  int g_localRebuildNodeRefusals = 0;

  void *localRebuildNodeRefusingBackendAlloc(std::size_t size,
                                             const loka::core::LokaAllocationSite &site)
  {
    if (g_localRebuildRefuseNode && std::strcmp(site.ownerTag, "NodeDefinition") == 0)
    {
      ++g_localRebuildNodeRefusals;
      return 0;
    }
    return new (std::nothrow) char[size];
  }

  // Whether the recomposing root declares its child this pass. Toggling it and
  // driving an external NODE_DIRTY_CHILD tick forces a LOCAL REBUILD (not an
  // attach) in which the newly appearing child is freshly created through the
  // contextless composition path.
  bool g_localRebuildShowChild = false;

  class LocalRebuildRefusalRootNode;
  typedef loka::app::scene::BoundaryPropsFor<LocalRebuildRefusalRootNode> LocalRebuildRefusalRootProps;

  class LocalRebuildRefusalRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<LocalRebuildRefusalRootNode,
                                                         LocalRebuildRefusalRootProps>
  {
  public:
    explicit LocalRebuildRefusalRootNode(const LocalRebuildRefusalRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<LocalRebuildRefusalRootNode,
                                                    LocalRebuildRefusalRootProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_localRebuildShowChild)
      {
        root << GateProbeDefinition();
      }
      composition.declare(root);
    }
  };

  // Nested-child local-rebuild refusal harness (#132 ruling 3, Codex P2 hole 1
  // one level deeper). The root-of-subtree case above is already caught by
  // materializeLocalRebuildNode's `if (!created)`. This harness exercises the
  // residual: the local rebuild introduces a NESTABLE child (an inner Fragment)
  // whose OWN grandchild create() is refused while the inner Fragment succeeds.
  // The contextless createNodeFromDefinition then returns a NON-NULL partial
  // subtree (the grandchild silently dropped), so `if (!created)` is false and
  // -- without the subtree result carrier -- the flag stays down. The
  // grandchild refusal is definition-driven (returns 0 on demand, standing in
  // for the gate-routed create() #140/S2b will make return 0) so exactly the
  // grandchild is refused, never the inner Fragment.
  bool g_nestedGrandchildRefuse = false;

  struct RefusableGrandchildDefinition
      : public loka::app::scene::NodeDefinition<GateProbeProps, GateProbeNode>
  {
    typedef loka::app::scene::NodeDefinition<GateProbeProps, GateProbeNode> BaseType;
    RefusableGrandchildDefinition() : BaseType() {}
    RefusableGrandchildDefinition(const RefusableGrandchildDefinition &other) : BaseType(other) {}
    virtual loka::app::scene::Node *create() const
    {
      if (g_nestedGrandchildRefuse)
      {
        return 0;
      }
      return BaseType::create();
    }
    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return new RefusableGrandchildDefinition(*this);
    }
  };

  class MaterializationResultProbeBoundary : public loka::app::scene::BoundaryNode
  {
  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &,
                                    loka::app::scene::ComposeEvent)
    {
    }
  };

  // Whether the recomposing root declares its nested subtree (inner Fragment +
  // grandchild) this pass. Toggling it and driving an external NODE_DIRTY_CHILD
  // tick forces a LOCAL REBUILD in which the whole inner subtree is freshly
  // materialized through one contextless materializeLocalRebuildNode call.
  bool g_nestedShowChild = false;

  class NestedLocalRebuildRefusalRootNode;
  typedef loka::app::scene::BoundaryPropsFor<NestedLocalRebuildRefusalRootNode>
      NestedLocalRebuildRefusalRootProps;

  class NestedLocalRebuildRefusalRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<NestedLocalRebuildRefusalRootNode,
                                                         NestedLocalRebuildRefusalRootProps>
  {
  public:
    explicit NestedLocalRebuildRefusalRootNode(const NestedLocalRebuildRefusalRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<NestedLocalRebuildRefusalRootNode,
                                                    NestedLocalRebuildRefusalRootProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_nestedShowChild)
      {
        // The introduced direct child is a nestable (inner Fragment) carrying a
        // grandchild -- so the whole subtree is materialized by a single
        // materializeLocalRebuildNode(innerFragment) call, with the grandchild
        // created BELOW a non-null root: the exact nested residual.
        loka::app::FragmentDefinition inner;
        inner << RefusableGrandchildDefinition();
        root << inner;
      }
      composition.declare(root);
    }
  };

  // Root-attach-refusal harness (#132 ruling 3, Codex P2 hole 2). The root
  // create() is refusable (to arm the initial white flag via a refused root),
  // and the attach compose declares a node-local state whose materialization
  // the state-refusing backend below can reject even though root create()
  // succeeds — exercising "root retry gets past create() but the attach
  // compose itself raises the allocation flag".
  bool g_attachRefuseState = false;
  int g_attachStateRefusals = 0;

  void *attachStateRefusingBackendAlloc(std::size_t size,
                                        const loka::core::LokaAllocationSite &site)
  {
    if (g_attachRefuseState && std::strncmp(site.ownerTag, "State", 5) == 0)
    {
      ++g_attachStateRefusals;
      return 0;
    }
    return new (std::nothrow) char[size];
  }

  class AttachRefusalRootNode;
  typedef loka::app::scene::BoundaryPropsFor<AttachRefusalRootNode> AttachRefusalRootProps;

  class AttachRefusalRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<AttachRefusalRootNode, AttachRefusalRootProps>
  {
  public:
    explicit AttachRefusalRootNode(const AttachRefusalRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<AttachRefusalRootNode, AttachRefusalRootProps>(props),
          state_()
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      WhiteFlagStatePayload initial = {{0}};
      composition.declareStates().state(this->state_, initial);
      composition.declare(loka::app::FragmentDefinition());
    }

    loka::app::scene::NodeState<WhiteFlagStatePayload> state_;
  };

  struct AttachRefusalRootDefinition
      : public loka::app::scene::BoundaryDefinition<AttachRefusalRootProps, AttachRefusalRootNode>
  {
    typedef loka::app::scene::BoundaryDefinition<AttachRefusalRootProps, AttachRefusalRootNode> BaseType;
    AttachRefusalRootDefinition() : BaseType() {}
    AttachRefusalRootDefinition(const AttachRefusalRootDefinition &other) : BaseType(other) {}
    virtual loka::app::scene::Node *create() const
    {
      if (g_refuseRootCreate)
      {
        return 0;
      }
      return BaseType::create();
    }
    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return new AttachRefusalRootDefinition(*this);
    }
  };

} // namespace


void testBoundaryArenaContracts()
{
  printf("\n==== [testBoundaryArenaContracts] start ====\n");
  testNodeArenaAlignmentAndCapacity();
  testNodeArenaRegistrationStampsOwner();
  testNodeArenaDestroysChildrenBeforeParents();
  testNodeArenaReleaseTombstonePreventsClearDoubleDestruction();
  testNodeArenaClearDestroysHeapChildOfArenaParentExactlyOnce();
  testStateArenaAllocation();
  testStateArenaReleaseAndClear();
  testStateArenaReserveMatchesBatchEstimate();
  testStateArenaGrowsAcrossOwnerBatches();
  testUnreservedStateFallsBackAfterReservedCapacityIsConsumed();
  printf("==== [testBoundaryArenaContracts] ok ====\n");
}

void testNodeArenaRetiredGenerationContracts()
{
  printf("\n==== [testNodeArenaRetiredGenerationContracts] start ====\n");
  testNodeArenaDetachAndDestroyRetiredGeneration();
  printf("==== [testNodeArenaRetiredGenerationContracts] ok ====\n");
}

void testStateArenaSlabsCrossAllocationGate()
{
  const loka::core::LokaAllocationSite blockSite("StateArena", "Block");
  const loka::core::LokaAllocationSite slabSite("StateArena", "slab");
#ifdef LOKA_LIFECYCLE_AUDIT
  const int blockLiveBefore = loka::core::LokaAllocAuditLiveCount(blockSite);
  const int slabLiveBefore = loka::core::LokaAllocAuditLiveCount(slabSite);
#endif
  g_gateBackendAllocCalls = 0;
  g_gateBackendFreeCalls = 0;
  loka::core::LokaAllocSetBackend(&countingGateBackendAlloc, &countingGateBackendFree);
  {
    TestStateOwner owner;
    loka::app::scene::NodeState<int> local;
    {
      loka::app::scene::NodeComposition::StateBatch batch(&owner);
      batch.state(local, 42);
    }
    assert(local.isValid());
    assert(local.dangerouslyMutableState()->isArenaAllocated());
    assert(local.get() == 42);
    // Red before #132 S2: slab acquisition bypassed the gate, so the backend
    // saw zero traffic. One Block header plus one slab cross the gate now.
    assert(g_gateBackendAllocCalls == 2);
#ifdef LOKA_LIFECYCLE_AUDIT
    assert(loka::core::LokaAllocAuditLiveCount(blockSite) == blockLiveBefore + 1);
    assert(loka::core::LokaAllocAuditLiveCount(slabSite) == slabLiveBefore + 1);
#endif
  }
  assert(g_gateBackendFreeCalls == g_gateBackendAllocCalls);
  loka::core::LokaAllocSetBackend(0, 0);
#ifdef LOKA_LIFECYCLE_AUDIT
  // Full teardown balanced the ledger: the leak detector covers arena slabs.
  assert(loka::core::LokaAllocAuditLiveCount(blockSite) == blockLiveBefore);
  assert(loka::core::LokaAllocAuditLiveCount(slabSite) == slabLiveBefore);
  loka::core::LokaAllocAuditCheckpoint("testStateArenaSlabsCrossAllocationGate");
#endif
}

void testNodeArenaSlabCrossesAllocationGate()
{
  const loka::core::LokaAllocationSite slabSite("NodeArena", "slab");
#ifdef LOKA_LIFECYCLE_AUDIT
  const int slabLiveBefore = loka::core::LokaAllocAuditLiveCount(slabSite);
#endif
  g_gateBackendAllocCalls = 0;
  g_gateBackendFreeCalls = 0;
  loka::core::LokaAllocSetBackend(&countingGateBackendAlloc, &countingGateBackendFree);
  {
    loka::app::scene::NodeArena arena;
    arena.reserve(64);
    assert(arena.hasCapacity());
    // Red before #132 S2: the node slab was a plain new[] that bypassed the
    // gate, so the backend saw zero traffic.
    assert(g_gateBackendAllocCalls == 1);
#ifdef LOKA_LIFECYCLE_AUDIT
    assert(loka::core::LokaAllocAuditLiveCount(slabSite) == slabLiveBefore + 1);
#endif
    arena.clear();
    assert(!arena.hasCapacity());
    assert(g_gateBackendFreeCalls == 1);
  }
  loka::core::LokaAllocSetBackend(0, 0);
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditLiveCount(slabSite) == slabLiveBefore);
  loka::core::LokaAllocAuditCheckpoint("testNodeArenaSlabCrossesAllocationGate");
#endif
}

void testHeapNodeCrossesAllocationGate()
{
  const loka::core::LokaAllocationSite nodeSite("NodeDefinition", "Node");
#ifdef LOKA_LIFECYCLE_AUDIT
  const int nodeLiveBefore = loka::core::LokaAllocAuditLiveCount(nodeSite);
#endif
  g_gateBackendAllocCalls = 0;
  g_gateBackendFreeCalls = 0;
  loka::core::LokaAllocSetBackend(&countingGateBackendAlloc, &countingGateBackendFree);
  {
    GateProbeDefinition definition;
    loka::app::scene::Node *node = definition.create();
    assert(node != 0);
    // Red before #132 S2b: create() was a plain `new NodeT` that bypassed the
    // gate, so the backend saw zero traffic and the node carried no gate
    // provenance. One heap node crosses the gate now.
    assert(node->isGateAllocated());
    assert(!node->isArenaAllocated());
    assert(g_gateBackendAllocCalls == 1);
#ifdef LOKA_LIFECYCLE_AUDIT
    assert(loka::core::LokaAllocAuditLiveCount(nodeSite) == nodeLiveBefore + 1);
#endif
    // The heap node returns to the backend through the same gate site.
    loka::app::scene::DestroyHeapNode(node);
    assert(g_gateBackendFreeCalls == 1);
#ifdef LOKA_LIFECYCLE_AUDIT
    assert(loka::core::LokaAllocAuditLiveCount(nodeSite) == nodeLiveBefore);
#endif
  }
  assert(g_gateBackendFreeCalls == g_gateBackendAllocCalls);
  loka::core::LokaAllocSetBackend(0, 0);
#ifdef LOKA_LIFECYCLE_AUDIT
  // Full teardown balanced the ledger for the heap-node site.
  loka::core::LokaAllocAuditCheckpoint("testHeapNodeCrossesAllocationGate");
#endif
}


/** #132 S3 red test (a): a driven recompose under a backend that refuses the
    slab (and the heap door behind it) must convert into a projection failure
    that waits for the next externally caused tick — previous platform content
    stands, snapshots invalidate (#70), and nothing self-schedules a retry. */
void testComposeAllocationWhiteFlagDefersFullRebuildToNextExternalTick()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<WhiteFlagRecomposeRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    WhiteFlagRecomposeRootNode *root = static_cast<WhiteFlagRecomposeRootNode *>(
        loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    assert(root);
    assert(root->first_.isValid());
    assert(root->second_.isValid());
    const size_t appliedBefore = platform.changeCount();
    assert(appliedBefore > 0);

    // Externally driven recompose while the backend refuses every gate
    // acquisition: the StateArena block append surrenders first, then the
    // gate-routed heap fallback for each redeclared state.
    g_refusingBackendRefusals = 0;
    loka::core::LokaAllocSetBackend(&refusingBackendAlloc, &delegatingBackendFree);
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    loka::core::LokaAllocSetBackend(0, 0);
    assert(g_refusingBackendRefusals > 0);

    // No state materialized, and none is half-alive: the out handles are
    // invalid instead of pointing at partially adopted storage.
    assert(!root->first_.isValid());
    assert(!root->second_.isValid());
    // Red before #132 S3: the dead states were adopted silently, the compose
    // completed as a success, and the half-alive cycle was projected (the
    // recorded change count grew). Now the previously applied content is
    // still the last thing the platform saw ...
    assert(platform.changeCount() == appliedBefore);
    // ... the compose window records the projection failure ...
    assert(!root->composeResult().composed);
    assert(root->composeResult().allocationFailed);
    // ... the composition snapshots are invalidated per the #70 mechanism ...
    assert(root->previousCompositionSnapshot().empty());
    assert(root->currentCompositionSnapshot().empty());
    // ... and the failure path did not self-schedule a tick. Without an
    // external drive the tracker stays silent: no second refresh cycle runs.
    assert(!scene.hasPendingInvalidation());
    assert(!scene.flushInvalidation());
    assert(platform.changeCount() == appliedBefore);

    // One externally caused tick with the default backend restored: the
    // recorded full rebuild rides it, recomposes from a clean slate, and the
    // healed content reaches the platform as a full-rebuild projection.
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());
    assert(root->first_.isValid());
    assert(root->second_.isValid());
    assert(root->composeResult().composed);
    assert(!root->composeResult().allocationFailed);
    assert(platform.changeCount() > appliedBefore);
    assert(platform.changeAt(platform.changeCount() - 1).fullRebuild);
  }
  loka::core::LokaAllocSetBackend(0, 0);
#ifdef LOKA_LIFECYCLE_AUDIT
  // Failure paths are where leaks nest: the whole scene lifetime, including
  // the refused cycle, must leave the audit ledger balanced.
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint(
      "testComposeAllocationWhiteFlagDefersFullRebuildToNextExternalTick");
#endif
}

/** #132 S3 red test (b): with the owner arena exhausted and the backend
    refusing the gate-routed heap fallback, state creation must raise the
    owner's white flag so the open compose window converts into a failure —
    instead of completing as a success around a silent dead state. */
void testHeapFallbackWhiteFlagFailsBoundaryCompose()
{
  const loka::core::LokaAllocationSite heapStateSite("StateOwner", "MutableState");
#ifdef LOKA_LIFECYCLE_AUDIT
  const int heapLiveBefore = loka::core::LokaAllocAuditLiveCount(heapStateSite);
#endif
  g_heapStateRefusals = 0;
  loka::core::LokaAllocSetBackend(&heapStateRefusingBackendAlloc, &delegatingBackendFree);
  {
    loka::app::scene::StdCompositionBoundaryNode owner((loka::app::scene::StdCompositionProps()));
    loka::app::scene::NodeState<LargeStateValue> reserved;
    loka::app::scene::NodeState<LargeStateValue> unreserved;
    LargeStateValue initial = {{0}};

    // The arena door works normally under the selective backend.
    owner.reserveStateArena(
        loka::app::scene::StateBatchBase::ArenaBytesForState<LargeStateValue>());
    loka::app::scene::StateBatchBase::CreateStateFromInitial<LargeStateValue>(
        &owner, reserved, initial);
    assert(reserved.isValid());
    assert(reserved.dangerouslyMutableState()->isArenaAllocated());

    // Reserved capacity is consumed; the next creation inside an open compose
    // window must take the heap door, which the backend refuses.
    owner.beginComposeResult(loka::app::scene::COMPOSE_EVENT_UPDATE,
                             loka::app::scene::NODE_DIRTY_PROPS);
    loka::app::scene::StateBatchBase::CreateStateFromInitial<LargeStateValue>(
        &owner, unreserved, initial);
    owner.completeComposeResult(true);
    assert(g_heapStateRefusals == 1);

    // No half-alive state: the out handle is invalid, nothing was adopted.
    assert(!unreserved.isValid());
    // Red before #132 S3: the compose window completed as a success and the
    // refusal was swallowed. Now the white flag converts it at completion.
    assert(!owner.composeResult().composed);
    assert(owner.composeResult().allocationFailed);
    // The state created before the failure is untouched by the conversion.
    assert(reserved.isValid());
  }
  loka::core::LokaAllocSetBackend(0, 0);
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditLiveCount(heapStateSite) == heapLiveBefore);
  loka::core::LokaAllocAuditCheckpoint("testHeapFallbackWhiteFlagFailsBoundaryCompose");
#endif
}

/** #132 ruling 3 / #140 P2 red test: a refused scene-root allocation must not
    crash or leave a permanent blank. The refused mount arms the scene white
    flag and leaves the scene uncomposed without self-scheduling a tick; the
    next externally caused refresh retries root creation and, once the backend
    has healed, builds the root, composes, and projects a full rebuild. Red
    before the Scene.hpp guard+retry: the refused root asserted/null-deref'd in
    ensureRootNode/composeIfNeeded, and (guarded) refreshComposition's
    !composed_ early-return stranded the scene as a permanent blank. */
void testSceneRootAllocationRefusalArmsWhiteFlagAndHealsOnRefresh()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  {
    SceneTestSupport::RecordingPlatformController platform;
    // Refuse the root allocation before the scene is mounted.
    g_refuseRootCreate = true;
    loka::app::scene::Scene scene((RefusableRootDefinition()));
    scene.mount(&platform);
    scene.updateAttached(true);

    // No crash, no assert. The root was never created, the scene stays
    // uncomposed, and the white flag is armed for a later external retry.
    assert(loka::dsl::testing::SceneTestAccess::rootBoundary(scene) == 0);
    assert(!loka::dsl::testing::SceneTestAccess::composed(scene));
    assert(loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    assert(platform.changeCount() == 0);

    // The failure path did not self-schedule a tick: a driverless flush does
    // nothing and the scene stays uncomposed, waiting for an external drive.
    assert(!scene.hasPendingInvalidation());
    assert(!scene.flushInvalidation());
    assert(!loka::dsl::testing::SceneTestAccess::composed(scene));
    assert(platform.changeCount() == 0);

    // Heal the backend and drive ONE externally caused refresh. The armed
    // white flag rides it: root creation retries, the root builds and
    // composes, and the healed content reaches the platform through the same
    // onChange projection a healthy mount uses (a full rebuild of a root that
    // never had prior platform state — so refreshComposition owes no diff
    // cycle and reports no apply-cycle change).
    g_refuseRootCreate = false;
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    assert(loka::dsl::testing::SceneTestAccess::rootBoundary(scene) != 0);
    assert(loka::dsl::testing::SceneTestAccess::composed(scene));
    assert(!loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    assert(platform.changeCount() > 0);
    assert(platform.changeAt(platform.changeCount() - 1).fullRebuild);
    // The scene is fully healthy: a further external drive runs a normal
    // update cycle through the now-existing root without re-arming the flag.
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    assert(loka::dsl::testing::SceneTestAccess::composed(scene));
    assert(!loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
  }
  g_refuseRootCreate = false;
#ifdef LOKA_LIFECYCLE_AUDIT
  // The whole scene lifetime, including the refused mount, must leave the
  // audit ledger balanced.
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint(
      "testSceneRootAllocationRefusalArmsWhiteFlagAndHealsOnRefresh");
#endif
}

/** #132 ruling 3 / Codex P2 (hole 1): a LOCAL REBUILD (not an attach) that
    freshly materializes a child through the contextless composition path must
    still raise the boundary white flag when that fresh create() is refused.
    The contextless temporary NodeComposition has no ComponentContext, so
    createNodeFromDefinition cannot reach the boundary to raise the flag itself
    — the materializeLocalRebuildNode helper raises it at the boundary-member
    call site. Without that raise the refused rebuild returns 0, the compose
    completes as a success (allocationFailed == false), and the scene applies an
    incomplete rebuild instead of deferring. With it, the compose converts into
    a projection failure: previous content stands, snapshots invalidate (#70),
    nothing self-schedules, and the next external tick heals from a clean slate. */
void testLocalRebuildNodeRefusalDefersFullRebuildToNextExternalTick()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  {
    SceneTestSupport::RecordingPlatformController platform;
    g_localRebuildShowChild = false;
    g_localRebuildRefuseNode = false;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<LocalRebuildRefusalRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    LocalRebuildRefusalRootNode *root = static_cast<LocalRebuildRefusalRootNode *>(
        loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    assert(root);
    assert(root->composeResult().composed);
    const size_t appliedBefore = platform.changeCount();
    assert(appliedBefore > 0);

    // Externally driven recompose that introduces a fresh child. The child's
    // gate-routed create() is refused, so the contextless local-rebuild
    // materialization returns 0.
    g_localRebuildShowChild = true;
    g_localRebuildRefuseNode = true;
    g_localRebuildNodeRefusals = 0;
    loka::core::LokaAllocSetBackend(&localRebuildNodeRefusingBackendAlloc, &delegatingBackendFree);
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    loka::core::LokaAllocSetBackend(0, 0);
    g_localRebuildRefuseNode = false;
    assert(g_localRebuildNodeRefusals > 0);

    // Red before the hole-1 flag-raise: the contextless refusal was dropped,
    // the compose completed as a success, and the incomplete rebuild applied.
    // Now the compose window records the projection failure ...
    assert(!root->composeResult().composed);
    assert(root->composeResult().allocationFailed);
    // ... the composition snapshots are invalidated per the #70 mechanism ...
    assert(root->previousCompositionSnapshot().empty());
    assert(root->currentCompositionSnapshot().empty());
    // ... the previously applied content still stands (nothing published) ...
    assert(platform.changeCount() == appliedBefore);
    // ... the scene recorded a deferred full rebuild ...
    assert(loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    // ... and the failure path did not self-schedule a tick.
    assert(!scene.hasPendingInvalidation());
    assert(!scene.flushInvalidation());
    assert(platform.changeCount() == appliedBefore);

    // One externally caused tick with the child now creatable: the recorded
    // full rebuild rides it, recomposes from a clean slate, and the healed
    // content reaches the platform as a full-rebuild projection.
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());
    assert(root->composeResult().composed);
    assert(!root->composeResult().allocationFailed);
    assert(!loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    assert(platform.changeCount() > appliedBefore);
    assert(platform.changeAt(platform.changeCount() - 1).fullRebuild);
  }
  loka::core::LokaAllocSetBackend(0, 0);
  g_localRebuildShowChild = false;
  g_localRebuildRefuseNode = false;
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint(
      "testLocalRebuildNodeRefusalDefersFullRebuildToNextExternalTick");
#endif
}

/** #132 ruling 3 / Codex P2 (hole 1, one level deeper): a LOCAL REBUILD that
    freshly materializes a NESTABLE child whose OWN grandchild create() is
    refused must still raise the boundary white flag. The prior fix routed a
    refused SUBTREE ROOT through materializeLocalRebuildNode's `if (!created)`,
    but a refused NESTED child returns a NON-NULL partial subtree (the
    grandchild is silently dropped by createNodeRecursive's child-root check),
    so `if (!created)` never fires. The subtree result carrier raises the flag
    at the owner-side choke point, converting the compose into a projection
    failure at ANY depth. Red before the carrier: the
    partial subtree is diffed/applied, the compose completes as a success
    (allocationFailed == false), and the incomplete rebuild reaches the
    platform. Green: previous content stands, snapshots invalidate (#70),
    nothing self-schedules, and the next external tick heals from a clean
    slate. This is the CONTEXTLESS local-rebuild path (not the initial attach,
    which already routes via context). */
void testNestedLocalRebuildChildRefusalDefersFullRebuildToNextExternalTick()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  {
    // Characterize the with-context arena recursion separately from the
    // contextless local-rebuild recursion below. The root and inner Fragment
    // materialize, while the grandchild's heap door refuses; the completed
    // fact must retain that nested refusal through both parent child loops.
    MaterializationResultProbeBoundary boundary;
    loka::app::scene::ComponentContext context;
    context.setBoundary(&boundary);
    loka::app::scene::NodeComposition composition;
    composition.setContext(&context);
    loka::app::FragmentDefinition inner;
    inner << RefusableGrandchildDefinition();
    loka::app::FragmentDefinition definition;
    definition << inner;
    boundary.nodeArena()->reserve(definition.nodeSize() +
                                  definition.nodeAlign());
    g_nestedGrandchildRefuse = true;
    loka::app::scene::NodeMaterializationResult result =
        loka::app::scene::testing::NodeCompositionTestAccess::
            createNodeFromDefinitionResult(composition, &definition);
    g_nestedGrandchildRefuse = false;
    assert(result.root != 0);
    assert(result.allocationFailed);
  }
  {
    SceneTestSupport::RecordingPlatformController platform;
    g_nestedShowChild = false;
    g_nestedGrandchildRefuse = false;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<NestedLocalRebuildRefusalRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    NestedLocalRebuildRefusalRootNode *root = static_cast<NestedLocalRebuildRefusalRootNode *>(
        loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    assert(root);
    assert(root->composeResult().composed);
    const size_t appliedBefore = platform.changeCount();
    assert(appliedBefore > 0);

    // Externally driven recompose that introduces a nested subtree (inner
    // Fragment + grandchild). The inner Fragment materializes fine but the
    // grandchild create() -- created BELOW the non-null inner Fragment through
    // the contextless composition -- is refused, so the subtree comes back a
    // non-null partial. Without the subtree result carrier the flag would be
    // dropped here.
    g_nestedShowChild = true;
    g_nestedGrandchildRefuse = true;
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    g_nestedGrandchildRefuse = false;

    // Red before the subtree result carrier: the nested refusal was dropped,
    // the compose completed as a success, and the incomplete rebuild applied.
    // Now the compose window records the projection failure ...
    assert(!root->composeResult().composed);
    assert(root->composeResult().allocationFailed);
    // ... the composition snapshots are invalidated per the #70 mechanism ...
    assert(root->previousCompositionSnapshot().empty());
    assert(root->currentCompositionSnapshot().empty());
    // ... the previously applied content still stands (nothing published) ...
    assert(platform.changeCount() == appliedBefore);
    // ... the scene recorded a deferred full rebuild ...
    assert(loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    // ... and the failure path did not self-schedule a tick.
    assert(!scene.hasPendingInvalidation());
    assert(!scene.flushInvalidation());
    assert(platform.changeCount() == appliedBefore);

    // One externally caused tick with the grandchild now creatable: the
    // recorded full rebuild rides it, recomposes from a clean slate, and the
    // healed content reaches the platform as a full-rebuild projection.
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());
    assert(root->composeResult().composed);
    assert(!root->composeResult().allocationFailed);
    assert(!loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    assert(platform.changeCount() > appliedBefore);
    assert(platform.changeAt(platform.changeCount() - 1).fullRebuild);
  }
  g_nestedShowChild = false;
  g_nestedGrandchildRefuse = false;
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint(
      "testNestedLocalRebuildChildRefusalDefersFullRebuildToNextExternalTick");
#endif
}

/** #132 ruling 3 / Codex P2 (hole 2): after a refused root create() arms the
    scene white flag, an external refresh retries. If root create() now
    succeeds but the attach compose itself refuses a node-local state, the
    compose is again a projection failure. composeIfNeeded must NOT publish a
    partial root or mark the scene composed: it must read the root boundary's
    allocationFailed and stay uncomposed so the re-armed white flag survives
    refreshComposition's `if (composed_) clear` block. Red before the hole-2
    guard: the partial root is published, composed_ is set, and the flag is
    cleared — losing the retry. A later fully-healed external tick composes
    cleanly. */
void testRootAttachAllocationRefusalKeepsWhiteFlagArmedForRetry()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  {
    SceneTestSupport::RecordingPlatformController platform;
    // Arm the scene white flag with a refused root create(): the scene mounts
    // uncomposed, exactly the state that drives refreshComposition's retry.
    g_refuseRootCreate = true;
    g_attachRefuseState = false;
    loka::app::scene::Scene scene((AttachRefusalRootDefinition()));
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(loka::dsl::testing::SceneTestAccess::rootBoundary(scene) == 0);
    assert(!loka::dsl::testing::SceneTestAccess::composed(scene));
    assert(loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    assert(platform.changeCount() == 0);

    // Retry: root create() now succeeds, but the attach compose refuses the
    // node-local state allocation. The retry gets past create() yet the compose
    // still raises the flag.
    g_refuseRootCreate = false;
    g_attachRefuseState = true;
    g_attachStateRefusals = 0;
    loka::core::LokaAllocSetBackend(&attachStateRefusingBackendAlloc, &delegatingBackendFree);
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    loka::core::LokaAllocSetBackend(0, 0);
    g_attachRefuseState = false;
    assert(g_attachStateRefusals > 0);

    // Red before the hole-2 guard: composeIfNeeded published a partial root and
    // set composed_, and refreshComposition then cleared the just-re-armed
    // flag. Now the scene stays uncomposed, nothing was published, and the
    // white flag is still armed for the next retry.
    assert(!loka::dsl::testing::SceneTestAccess::composed(scene));
    assert(loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    assert(platform.changeCount() == 0);
    // No self-scheduled tick: a driverless flush does nothing.
    assert(!scene.hasPendingInvalidation());
    assert(!scene.flushInvalidation());
    assert(!loka::dsl::testing::SceneTestAccess::composed(scene));
    assert(platform.changeCount() == 0);

    // Heal the backend and drive one more external refresh: the retry now gets
    // past the attach without raising the flag, the root composes, the flag is
    // consumed, the refused node-local state materializes, and content reaches
    // the platform.
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    assert(loka::dsl::testing::SceneTestAccess::composed(scene));
    assert(!loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    AttachRefusalRootNode *healedRoot = static_cast<AttachRefusalRootNode *>(
        loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    assert(healedRoot && healedRoot->composeResult().composed);
    assert(!healedRoot->composeResult().allocationFailed);
    // The previously refused state is now live: the content healed.
    assert(healedRoot->state_.isValid());
    assert(platform.changeCount() > 0);
  }
  loka::core::LokaAllocSetBackend(0, 0);
  g_refuseRootCreate = false;
  g_attachRefuseState = false;
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint(
      "testRootAttachAllocationRefusalKeepsWhiteFlagArmedForRetry");
#endif
}
