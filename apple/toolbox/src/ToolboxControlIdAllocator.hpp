#ifndef LOKA_TOOLBOX_CONTROL_ID_ALLOCATOR_HPP
#define LOKA_TOOLBOX_CONTROL_ID_ALLOCATOR_HPP

#include <vector>

/** Auto control-id policy for the Toolbox projection seam.

    Pairing between logical nodes and Toolbox controls must be by identity:
    an auto id, once handed to a context, stays that context's id until the
    context releases it. Ids are therefore issued monotonically for the
    controller's lifetime and recycled through a free list — never by
    resetting the counter (a per-frame reset made lazily-allocating contexts
    collide as soon as a Show reveal introduced new controls; see issue #120).

    Explicit control tags live below the auto range by convention. When a
    larger explicit tag is observed, raiseBaseAbove() lifts the auto range
    above it; the base only ever rises, and freed ids below the current base
    are discarded instead of reissued. */
class ToolboxControlIdAllocator
{
public:
  explicit ToolboxControlIdAllocator(short autoBaseId)
      : base_(autoBaseId),
        next_(autoBaseId),
        freeIds_()
  {
  }

  /** Lifts the auto range above an observed explicit id. Never lowers. */
  void raiseBaseAbove(short explicitId)
  {
    if (explicitId >= base_)
    {
      base_ = static_cast<short>(explicitId + 1);
      if (next_ < base_)
      {
        next_ = base_;
      }
    }
  }

  short allocate()
  {
    while (!freeIds_.empty())
    {
      short id = freeIds_.back();
      freeIds_.pop_back();
      if (id >= base_)
      {
        return id;
      }
      /* Freed before the base rose past it: an explicit tag may now own
         this value, so it is discarded rather than reissued. */
    }
    if (next_ < base_)
    {
      next_ = base_;
    }
    return next_++;
  }

  /** Returns an id to the pool. Only ids this allocator issued are accepted;
      explicit tags routed through a shared destroy path are ignored here and
      the base guard in allocate() keeps any misrouted value from being
      reissued into a live range. */
  void release(short id)
  {
    if (id < base_ || id >= next_)
    {
      return;
    }
    freeIds_.push_back(id);
  }

  short peekNextForTest() const
  {
    return next_;
  }

private:
  short base_;
  short next_;
  std::vector<short> freeIds_;
};

#endif // LOKA_TOOLBOX_CONTROL_ID_ALLOCATOR_HPP
