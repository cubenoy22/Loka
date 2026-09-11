#include "KeyedGenerationStorageTests.hpp"
#include "support/TestVerify.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/Keyed.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "core/LokaAlloc.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include <cstdio>
#include <cstring>
#include <map>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;

  struct StorageCount
  {
    StorageCount()
        : allocations(0),
          frees(0),
          bytes(0)
    {
    }
    int allocations, frees;
    std::size_t bytes;
    int outstanding() const
    {
      return this->allocations - this->frees;
    }
  };

  /** Owns the backend's size ledger; callbacks borrow it only for this test. */
  class AllocationProbe
  {
  public:
    AllocationProbe();
    ~AllocationProbe();
    StorageCount blocks, slabs, heap;
    std::map<void *, std::size_t> live;

    StorageCount *count(const loka::core::LokaAllocationSite &site)
    {
      if (std::strcmp(site.ownerTag, "StateArena") == 0)
      {
        if (std::strcmp(site.typeTag, "Block") == 0)
          return &this->blocks;
        if (std::strcmp(site.typeTag, "slab") == 0)
          return &this->slabs;
      }
      if (std::strcmp(site.ownerTag, "StateOwner") == 0)
        return &this->heap;
      return 0;
    }

  private:
    AllocationProbe(const AllocationProbe &);
    AllocationProbe &operator=(const AllocationProbe &);
  };
  AllocationProbe *probe = 0;

  void *allocate(std::size_t size, const loka::core::LokaAllocationSite &site)
  {
    void *ptr = new (std::nothrow) char[size];
    if (ptr)
    {
      probe->live[ptr] = size;
      StorageCount *count = probe->count(site);
      if (count)
      {
        ++count->allocations;
        count->bytes += size;
      }
    }
    return ptr;
  }
  void freeAllocation(void *ptr, const loka::core::LokaAllocationSite &site)
  {
    std::map<void *, std::size_t>::iterator found = probe->live.find(ptr);
    LOKA_VERIFY(found != probe->live.end());
    StorageCount *count = probe->count(site);
    if (count)
    {
      ++count->frees;
      count->bytes -= found->second;
    }
    probe->live.erase(found);
    delete[] static_cast<char *>(ptr);
  }
  AllocationProbe::AllocationProbe()
  {
    LOKA_VERIFY(probe == 0);
    probe = this;
    loka::core::LokaAllocSetBackend(&allocate, &freeAllocation);
  }
  AllocationProbe::~AllocationProbe()
  {
    loka::core::LokaAllocSetBackend(0, 0);
    probe = 0;
  }

  struct ComponentLifetime;
  ComponentLifetime *activeLifetime = 0;
  struct ComponentLifetime
  {
    ComponentLifetime()
        : constructed(0),
          destroyed(0)
    {
      activeLifetime = this;
    }
    ~ComponentLifetime()
    {
      activeLifetime = 0;
    }
    int constructed, destroyed;
  };
  class ResidentNode;
  struct ResidentTag
  {
  };
  struct ResidentProps : NodePropsBase<ResidentProps>
  {
    typedef ResidentTag TypeTag;
    typedef ResidentNode NodeType;
    explicit ResidentProps(ComponentLifetime *value)
        : lifetime(value)
    {
    }
    bool operator<(const PropsBase &) const
    {
      return false;
    }
    ComponentLifetime *lifetime;
  };
  class ResidentNode : public ComponentNodeWithProps<ResidentProps>
  {
  public:
    explicit ResidentNode(const ResidentProps &p)
        : ComponentNodeWithProps<ResidentProps>(p)
    {
      ++this->props.lifetime->constructed;
      this->state(this->value_, 7);
    }
    virtual ~ResidentNode()
    {
      ++this->props.lifetime->destroyed;
    }
    virtual void composeChildren(NodeComposition &)
    {
      LOKA_VERIFY(this->value_.isValid());
      LOKA_VERIFY(this->value_.get() == 7);
    }

  private:
    NodeState<int> value_;
  };

  template <bool WithSection> class Root : public BoundaryNodeFor<Root<WithSection> >
  {
  public:
    explicit Root(const BoundaryPropsFor<Root<WithSection> > &p)
        : BoundaryNodeFor<Root<WithSection> >(p)
    {
      this->state(this->key, 0);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Fragment() << Keyed(*this->key.state(), this, &Root::declareArm));
    }
    void declareArm(NodeComposition &c)
    {
      if (WithSection)
        c.declare(Fragment() << (Section(635) << Component(ResidentProps(activeLifetime))));
      else
        c.declare(Fragment() << Component(ResidentProps(activeLifetime)));
    }
    NodeState<int> key;
  };

  struct Snapshot
  {
    explicit Snapshot(const AllocationProbe &p)
        : blocks(p.blocks.outstanding()),
          slabs(p.slabs.outstanding()),
          bytes(p.slabs.bytes),
          heap(p.heap.outstanding())
    {
    }
    bool operator==(const Snapshot &other) const
    {
      return this->blocks == other.blocks && this->slabs == other.slabs && this->bytes == other.bytes
             && this->heap == other.heap;
    }
    int blocks, slabs;
    std::size_t bytes;
    int heap;
  };

  template <bool WithSection>
  void flipThrough(Scene &scene, Root<WithSection> &root, const ComponentLifetime &lifetime, int first, int last)
  {
    for (int key = first; key <= last; ++key)
    {
      {
        loka::core::StateTrackerGuard guard(root.tracker());
        root.key.set(key);
      }
      scene.flushInvalidation();
      const bool sameBoundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene) == &root;
      LOKA_VERIFY(sameBoundary);
      LOKA_VERIFY(lifetime.constructed == key + 1);
      LOKA_VERIFY(lifetime.destroyed == key);
    }
  }

  template <bool WithSection> void storagePlateaus()
  {
    AllocationProbe allocations;
    ComponentLifetime lifetime;
    bool plateau = false;
    {
      NullScenePlatformController platform;
      Scene scene((Boundary<Root<WithSection> >()));
      scene.mount(&platform);
      scene.updateAttached(true);
      Root<WithSection> *root =
          static_cast<Root<WithSection> *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
      LOKA_VERIFY(root != 0);
      flipThrough(scene, *root, lifetime, 1, 2);
      const Snapshot warm(allocations);
      flipThrough(scene, *root, lifetime, 3, 32);
      const Snapshot middle(allocations);
      flipThrough(scene, *root, lifetime, 33, 64);
      const Snapshot end(allocations);
      std::printf("Keyed %s at flips 2/32/64: blocks=%d/%d/%d slabs=%d/%d/%d "
                  "slab-bytes=%lu/%lu/%lu heap=%d/%d/%d destroyed=%d\n",
                  WithSection ? "Section" : "Component",
                  warm.blocks,
                  middle.blocks,
                  end.blocks,
                  warm.slabs,
                  middle.slabs,
                  end.slabs,
                  static_cast<unsigned long>(warm.bytes),
                  static_cast<unsigned long>(middle.bytes),
                  static_cast<unsigned long>(end.bytes),
                  warm.heap,
                  middle.heap,
                  end.heap,
                  lifetime.destroyed);
      std::fflush(stdout);
      plateau = warm == middle && warm == end;
      const bool noNativeControls = platform.ledger().empty();
      LOKA_VERIFY(noNativeControls);
    }
    LOKA_VERIFY(lifetime.constructed == 65 && lifetime.destroyed == 65);
    LOKA_VERIFY(allocations.live.empty());
    LOKA_VERIFY(allocations.blocks.outstanding() == 0);
    LOKA_VERIFY(allocations.slabs.outstanding() == 0 && allocations.slabs.bytes == 0);
    LOKA_VERIFY(allocations.heap.outstanding() == 0);
    // Logical retirement has completed at every checkpoint; retained arena
    // slabs must therefore plateau while the same Boundary remains mounted.
    LOKA_VERIFY(plateau);
  }
} // namespace

void testKeyedSectionGenerationStoragePlateaus()
{
  storagePlateaus<true>();
}
void testKeyedComponentGenerationStoragePlateaus()
{
  storagePlateaus<false>();
}
