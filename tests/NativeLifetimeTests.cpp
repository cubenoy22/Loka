#include "NativeLifetimeTests.hpp"

#include <cassert>

#include "app/scene/Node.hpp"
#include "app/scene/projection/NativeHandlePool.hpp"
#include "app/scene/projection/NativeNodeContext.hpp"

// Pins for the W3-2 native table hint axis (#98): the one wish a Node may
// express about its native pair travels declare -> definition -> node ->
// native context as observable state, and the exact-match bucket earns its
// performance through counters, never adaptation.

namespace
{
  class HintProbeNode;
  struct HintProbeTypeTag
  {
  };

  struct HintProbeProps : public loka::app::scene::NodePropsBase<HintProbeProps>
  {
    typedef HintProbeTypeTag TypeTag;
    typedef HintProbeNode NodeType;
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
  };

  class HintProbeNode : public loka::app::scene::Node
  {
  public:
    typedef HintProbeTypeTag TypeTag;
    HintProbeProps props;
    HintProbeNode(const HintProbeProps &p)
        : props(p)
    {
    }
  };

  struct HintProbeDefinition : public loka::app::scene::NodeDefinition<HintProbeProps, HintProbeNode>
  {
  };

  int g_disposedHandleSum = 0;

  void recordDisposedHandle(int handle)
  {
    g_disposedHandleSum += handle;
  }
} // namespace

void testNodeDefaultsToDefaultNativeLifetimeHint()
{
  HintProbeDefinition definition;
  loka::app::scene::Node *node = definition.create();
  assert(node);
  assert(node->nativeLifetimeHint() == loka::app::scene::NATIVE_HINT_DEFAULT);
  node->setNativeLifetimeHint(loka::app::scene::NATIVE_HINT_EAGER_RELEASE);
  assert(node->nativeLifetimeHint() == loka::app::scene::NATIVE_HINT_EAGER_RELEASE);
  delete node;
}

void testDefinitionCarriesNativeLifetimeHintToCreatedNode()
{
  HintProbeDefinition definition;
  definition.lifetimeHint(loka::app::scene::NATIVE_HINT_DESIRE_STAY);
  loka::app::scene::Node *node = definition.create();
  assert(node);
  assert(node->nativeLifetimeHint() == loka::app::scene::NATIVE_HINT_DESIRE_STAY);
  delete node;
}

void testDefinitionCloneAndApplyPreserveNativeLifetimeHint()
{
  HintProbeDefinition definition;
  definition.lifetimeHint(loka::app::scene::NATIVE_HINT_EAGER_RELEASE);

  loka::app::scene::NodeDefinitionBase *clone = definition.clone();
  assert(clone);
  assert(clone->nativeLifetimeHint() == loka::app::scene::NATIVE_HINT_EAGER_RELEASE);

  loka::app::scene::Node *node = clone->create();
  assert(node);
  assert(node->nativeLifetimeHint() == loka::app::scene::NATIVE_HINT_EAGER_RELEASE);

  // A props re-apply from a definition with a different hint refreshes the
  // node's hint alongside the tag, mirroring setNodeTag.
  HintProbeDefinition updated;
  updated.lifetimeHint(loka::app::scene::NATIVE_HINT_DESIRE_STAY);
  bool applied = updated.applyPropsToNode(node);
  assert(applied);
  assert(node->nativeLifetimeHint() == loka::app::scene::NATIVE_HINT_DESIRE_STAY);

  delete node;
  delete clone;
}

void testNativeContextObservesLifetimeHint()
{
  loka::app::scene::NativeNodeContext context;
  assert(context.lifetimeHint() == loka::app::scene::NATIVE_HINT_DEFAULT);
  context.observeLifetimeHint(loka::app::scene::NATIVE_HINT_EAGER_RELEASE);
  assert(context.lifetimeHint() == loka::app::scene::NATIVE_HINT_EAGER_RELEASE);
}

void testExactMatchBucketCountsHitsMissesEvictsAndDepth()
{
  loka::app::scene::ExactMatchHandleBucket<int> bucket;
  int handle = 0;

  assert(!bucket.tryAcquire(handle));
  assert(bucket.missCount() == 1);
  assert(bucket.hitCount() == 0);
  assert(bucket.depth() == 0);

  bucket.offer(7);
  bucket.offer(9);
  assert(bucket.depth() == 2);

  assert(bucket.tryAcquire(handle));
  assert(handle == 9);
  assert(bucket.hitCount() == 1);
  assert(bucket.depth() == 1);

  g_disposedHandleSum = 0;
  bucket.drainWith(recordDisposedHandle);
  assert(g_disposedHandleSum == 7);
  assert(bucket.evictCount() == 1);
  assert(bucket.depth() == 0);

  assert(!bucket.tryAcquire(handle));
  assert(bucket.missCount() == 2);
}
