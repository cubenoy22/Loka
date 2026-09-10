#include "core/SmallObjectPool.hpp"
struct Source
{
  enum { kAlignment = 16 };
  static void *acquire(size_t);
  static void release(void *);
};
loka::core::SmallObjectPool<Source> pool;
void pin()
{
  pool.release(pool.allocate(8));
}
