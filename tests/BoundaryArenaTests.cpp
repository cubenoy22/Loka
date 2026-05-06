#include "BoundaryArenaTests.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include "app/scene/boundary/detail/BoundaryArena.hpp"
#include "loka/core/State.hpp"

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

static void testNodeArenaAlignmentAndCapacity()
{
  const size_t minAlign = loka::app::scene::NodeArena::normalizeAlign(1);
  assert(minAlign >= sizeof(void *));
  assert(isPowerOfTwo(minAlign));

  const size_t roundedAlign = loka::app::scene::NodeArena::normalizeAlign(3);
  assert(roundedAlign >= 3);
  assert(isPowerOfTwo(roundedAlign));

  loka::app::scene::NodeArena arena;
  assert(!arena.hasCapacity());
  assert(arena.allocate(4, 4) == 0);

  arena.reserve(32);
  assert(arena.hasCapacity());

  void *first = arena.allocate(4, 4);
  assert(first != 0);
  assert(isAligned(first, loka::app::scene::NodeArena::normalizeAlign(4)));

  void *second = arena.allocate(8, 8);
  assert(second != 0);
  assert(isAligned(second, loka::app::scene::NodeArena::normalizeAlign(8)));
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

static void testStateArenaAllocation()
{
  loka::app::scene::StateArena arena;
  assert(!arena.hasCapacity());
  assert(arena.allocate(4, 4) == 0);

  arena.reserve(32);
  assert(arena.hasCapacity());

  void *first = arena.allocate(4, 4);
  assert(first != 0);
  assert(isAligned(first, loka::app::scene::NodeArena::normalizeAlign(4)));

  void *second = arena.allocate(8, 8);
  assert(second != 0);
  assert(isAligned(second, loka::app::scene::NodeArena::normalizeAlign(8)));
  assert(second != first);

  assert(arena.allocate(1024, 4) == 0);

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

} // namespace

void testBoundaryArenaContracts()
{
  printf("\n==== [testBoundaryArenaContracts] start ====\n");
  testNodeArenaAlignmentAndCapacity();
  testStateArenaAllocation();
  testStateArenaReleaseAndClear();
  printf("==== [testBoundaryArenaContracts] ok ====\n");
}
