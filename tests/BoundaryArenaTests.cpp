#include "BoundaryArenaTests.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <vector>
#include "app/scene/boundary/detail/BoundaryArena.hpp"
#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/state/StateBatchBase.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"

namespace
{

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
        delete state;
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
          delete state;
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

  static void testStateArenaAllocation()
  {
    loka::app::scene::StateArena arena;
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

    // Allocation may consume reserved capacity, but only reserve() may grow the
    // owner-lifetime arena. Unreserved state can then fall back to the heap.
    assert(arena.allocate(1024, 4) == 0);

    arena.reserve(1024);
    void *grown = arena.allocate(1024, 4);
    assert(grown != 0);
    assert(isAligned(grown, loka::app::scene::detail::NormalizeArenaAlign(4)));

    arena.clear();
    assert(!arena.hasCapacity());
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

    loka::app::scene::StateBatchBase::CreateImmediateState<LargeStateValue>(
        &owner, immediate, initial);
    assert(immediate.dangerouslyMutableState()->isArenaAllocated());
  }

} // namespace

void testBoundaryArenaContracts()
{
  printf("\n==== [testBoundaryArenaContracts] start ====\n");
  testNodeArenaAlignmentAndCapacity();
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
