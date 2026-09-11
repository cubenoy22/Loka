#include "KeyedGenerationStorageTests.hpp"
#include "support/TestVerify.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/Keyed.hpp"
#include "app/nodes/nestable/For.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/scene/Scene.hpp"
#include "app/nodes/Text.hpp"
#include "app/scene/boundary/LazyScopeDefinition.hpp"
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
    int heapRequestsBeforeRefusal, heapRefusals;
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
    if (std::strcmp(site.ownerTag, "StateOwner") == 0 && probe->heapRequestsBeforeRefusal >= 0)
    {
      if (probe->heapRequestsBeforeRefusal-- == 0)
      {
        ++probe->heapRefusals;
        return 0;
      }
    }
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
      : heapRequestsBeforeRefusal(-1),
        heapRefusals(0)
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
    loka::core::State<int> *valueState() const
    {
      return this->value_.state();
    }
    virtual void composeChildren(NodeComposition &)
    {
      LOKA_VERIFY(this->value_.isValid());
      LOKA_VERIFY(this->value_.get() == 7);
      const bool heap = !this->value_.state()->isArenaAllocated() && this->value_.state()->isGateAllocated();
      LOKA_VERIFY(heap);
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
#ifdef LOKA_LIFECYCLE_AUDIT
      const int ledgerHeap = loka::core::LokaAllocAuditLiveCount(HeapStateAllocationSite());
      LOKA_VERIFY(ledgerHeap == this->heap);
#endif
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

namespace
{
  class NestedRoot : public BoundaryNodeFor<NestedRoot>
  {
  public:
    explicit NestedRoot(const BoundaryPropsFor<NestedRoot> &p)
        : BoundaryNodeFor<NestedRoot>(p)
    {
      this->declareStates().state(this->outer, 0).state(this->inner, 0).state(this->shown, true);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Show(*this->shown.state()).destroyOnDetach()
                << Keyed(*this->outer.state(), this, &NestedRoot::declareOuter));
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
    void declareOuter(NodeComposition &c)
    {
      c.declare(Fragment() << (Section(6401) << Component(ResidentProps(activeLifetime)))
                           << (Section(6402) << Keyed(*this->inner.state(), this, &NestedRoot::declareInner)));
    }
    void declareInner(NodeComposition &c)
    {
      c.declare(Section(6403) << (Section(6404) << Component(ResidentProps(activeLifetime))));
    }
    NodeState<int> outer, inner;
    NodeState<bool> shown;
  };

  template <class T> void writeState(IStateOwner &owner, NodeState<T> &state, const T &value)
  {
    loka::core::StateTrackerGuard guard(owner.tracker());
    state.set(value);
  }

  /** Drive the ordinary Boundary update door without entering the Scene clock,
      so several completed replacements can precede one retirement drain. */
  template <class T> void replaceWithoutDrain(Scene &scene, BoundaryNode &root, NodeState<T> &state, const T &value)
  {
    writeState(root, state, value);
    ComponentContext context;
    context.setBoundary(&root);
    context.setStateOwner(&root);
    context.setScene(&scene);
    context.setPlatformController(loka::dsl::testing::SceneTestAccess::platformController(scene));
    context.setDirtyFlags(NODE_DIRTY_CHILD);
    BoundaryNode::composeSubtree(&root, context, COMPOSE_EVENT_UPDATE, 0);
  }

  class BatchedRoot : public Root<true>
  {
  public:
    explicit BatchedRoot(const BoundaryPropsFor<Root<true> > &p)
        : Root<true>(p)
    {
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
  };

  void verifyBalanced(const AllocationProbe &p)
  {
    LOKA_VERIFY(p.live.empty());
    LOKA_VERIFY(p.heap.outstanding() == 0);
    LOKA_VERIFY(p.blocks.outstanding() == 0 && p.slabs.outstanding() == 0 && p.slabs.bytes == 0);
  }

  void nestedRetirement(bool destroyOuter)
  {
    AllocationProbe allocations;
    ComponentLifetime lifetime;
    {
      NullScenePlatformController platform;
      Scene scene((Boundary<NestedRoot>()));
      scene.mount(&platform);
      scene.updateAttached(true);
      NestedRoot *root = static_cast<NestedRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
      const Snapshot warm(allocations);
      LOKA_VERIFY(warm.heap == 2);
      for (int i = 1; i <= 32; ++i)
      {
        replaceWithoutDrain(scene, *root, root->inner, i);
        scene.flushInvalidation();
        LOKA_VERIFY(Snapshot(allocations) == warm);
      }
      replaceWithoutDrain(scene, *root, root->outer, 1);
      scene.flushInvalidation();
      LOKA_VERIFY(Snapshot(allocations) == warm);
      LOKA_VERIFY(lifetime.constructed == 36 && lifetime.destroyed == 34);
      std::printf("Nested Keyed after 32 inner flips and outer replacement: heap=%d blocks=%d slabs=%d bytes=%lu\n",
                  warm.heap,
                  warm.blocks,
                  warm.slabs,
                  static_cast<unsigned long>(warm.bytes));
      // Three inner replacements stay retired but undrained. Their Sections
      // still borrow the outer generation provider until the FIFO clock runs.
      for (int i = 33; i <= 35; ++i)
        replaceWithoutDrain(scene, *root, root->inner, i);
      LOKA_VERIFY(lifetime.constructed == 39 && lifetime.destroyed == 34);
      LOKA_VERIFY(allocations.heap.outstanding() == 5);
      replaceWithoutDrain(scene, *root, root->outer, 2);
      LOKA_VERIFY(lifetime.constructed == 41 && lifetime.destroyed == 34);
      LOKA_VERIFY(allocations.heap.outstanding() == 7);
      if (destroyOuter)
      {
        replaceWithoutDrain(scene, *root, root->shown, false);
        LOKA_VERIFY(lifetime.destroyed == 34);
        scene.flushInvalidation();
        LOKA_VERIFY(lifetime.constructed == lifetime.destroyed);
        LOKA_VERIFY(allocations.heap.outstanding() == 0);
      }
      // Otherwise Boundary teardown must drain both old inner arms and outer
      // generations, including the currently installed two-state generation.
    }
    LOKA_VERIFY(lifetime.constructed == lifetime.destroyed);
    verifyBalanced(allocations);
  }

  class BoardCellNode;
  struct BoardCellTag
  {
  };
  struct BoardCellProps : NodePropsBase<BoardCellProps>
  {
    typedef BoardCellTag TypeTag;
    typedef BoardCellNode NodeType;
    bool operator<(const PropsBase &) const
    {
      return false;
    }
  };
  class BoardCellNode : public ComponentNodeWithProps<BoardCellProps>
  {
  public:
    explicit BoardCellNode(const BoardCellProps &p)
        : ComponentNodeWithProps<BoardCellProps>(p)
    {
      ++activeLifetime->constructed;
      this->state(this->value, loka::core::String::Literal("covered"));
    }
    virtual ~BoardCellNode()
    {
      ++activeLifetime->destroyed;
    }
    virtual const void *nodeTypeKey() const
    {
      return NodeTypeToken<BoardCellNode>();
    }
    virtual void declareBindings(BindingToken &token)
    {
      token.action(this->click, this, &BoardCellNode::reveal);
    }
    virtual void composeChildren(NodeComposition &c)
    {
      const bool heap = !this->value.state()->isArenaAllocated() && this->value.state()->isGateAllocated();
      LOKA_VERIFY(heap);
      c.declare(Cell(this->value.state()).onClick(&this->click));
    }
    void reveal()
    {
      this->value.set(loka::core::String::Literal("revealed"));
    }
    NodeState<loka::core::String> value;
    loka::core::EmitterState click;
  };
  class BoardRoot : public BoundaryNodeFor<BoardRoot>
  {
  public:
    explicit BoardRoot(const BoundaryPropsFor<BoardRoot> &p)
        : BoundaryNodeFor<BoardRoot>(p)
    {
      this->state(this->key, 0);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Fragment() << Keyed(*this->key.state(), this, &BoardRoot::declareBoard));
    }
    void declareBoard(NodeComposition &c)
    {
      const BoardCellProps items[16] = {};
      c.declare(Fragment() << For(6500, items, ComponentItemFactory<BoardCellProps>()));
    }
    NodeState<int> key;
  };

  Node *findType(Node *node, const void *type)
  {
    if (!node || node->nodeTypeKey() == type)
      return node;
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
    {
      Node *found = findType(child, type);
      if (found)
        return found;
    }
    return 0;
  }
} // namespace

void testNestedKeyedGenerationStoragePlateausAndOuterDestroyDrains()
{
  nestedRetirement(true);
}

void testNestedKeyedBoundaryTeardownDrainsRetiredGenerations()
{
  nestedRetirement(false);
}

void testKeyedMultipleReplacementsBeforeSingleDrain()
{
  AllocationProbe allocations;
  ComponentLifetime lifetime;
  {
    NullScenePlatformController platform;
    Scene scene((BoundaryDefinition<BoundaryPropsFor<Root<true> >, BatchedRoot>((BoundaryPropsFor<Root<true> >()))));
    scene.mount(&platform);
    scene.updateAttached(true);
    Root<true> *root = static_cast<Root<true> *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    const Snapshot warm(allocations);
    for (int i = 1; i <= 8; ++i)
      replaceWithoutDrain(scene, *root, root->key, i);
    LOKA_VERIFY(lifetime.constructed == 9 && lifetime.destroyed == 0);
    LOKA_VERIFY(allocations.heap.outstanding() == 9);
    scene.flushInvalidation();
    LOKA_VERIFY(lifetime.destroyed == 8);
    LOKA_VERIFY(Snapshot(allocations) == warm);
  }
  verifyBalanced(allocations);
}

void testKeyedForSectionBoardStoragePlateausAndClicksStayLive()
{
  AllocationProbe allocations;
  ComponentLifetime lifetime;
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<BoardRoot>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    BoardRoot *root = static_cast<BoardRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    const Snapshot warm(allocations);
    LOKA_VERIFY(warm.heap == 16);
    for (int i = 0; i <= 32; ++i)
    {
      if (i)
      {
        writeState(*root, root->key, i);
        scene.flushInvalidation();
      }
      BoardCellNode *cell = static_cast<BoardCellNode *>(findType(root, NodeTypeToken<BoardCellNode>()));
      LOKA_VERIFY(cell != 0);
      LOKA_VERIFY(cell->value.get().equals(loka::core::String::Literal("covered")));
      CellNode *control = static_cast<CellNode *>(findType(cell, NodeTypeToken<CellNode>()));
      LOKA_VERIFY(control != 0 && control->props.onClick_ != 0);
      BoundarySectionNode *section =
          static_cast<BoundarySectionNode *>(findType(root, NodeTypeToken<BoundarySectionNode>()));
      LOKA_VERIFY(section != 0 && section->stateStorageOwner() != root);
      {
        loka::core::StateTrackerGuard guard(section->tracker());
        control->props.onClick_->emit();
      }
      scene.flushInvalidation();
      LOKA_VERIFY(cell->value.get().equals(loka::core::String::Literal("revealed")));
      LOKA_VERIFY(Snapshot(allocations) == warm);
      LOKA_VERIFY(lifetime.constructed == (i + 1) * 16 && lifetime.destroyed == i * 16);
    }
  }
  LOKA_VERIFY(lifetime.constructed == 528 && lifetime.destroyed == 528);
  verifyBalanced(allocations);
}

void testUnboundSectionRefusesStateAndMaterialization()
{
  AllocationProbe allocations;
  {
    BoundarySectionNode section((SectionProps(635)));
    NodeState<int> state;
    StateBatchBase::CreateImmediateState(&section, state, 7);
    LOKA_VERIFY(!state.isValid());
    LOKA_VERIFY(allocations.heap.allocations == 0 && allocations.slabs.allocations == 0);
    NodeComposition composition;
    Section definition(635);
    NodeMaterializationResult result =
        testing::NodeCompositionTestAccess::createNodeFromDefinitionResult(composition, &definition);
    LOKA_VERIFY(result.allocationFailed);
    DestroyHeapNode(result.root);
  }
  verifyBalanced(allocations);
}

namespace
{
  class StorageBoundary : public BoundaryNode
  {
    virtual void composeWithContext(ComponentContext &, ComposeEvent) {}
  };

  class ParkedRoot : public Root<true>
  {
  public:
    explicit ParkedRoot(const BoundaryPropsFor<Root<true> > &p)
        : Root<true>(p)
    {
      this->state(this->shown, true);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Fragment() << (Show(*this->shown.state())
                               << Keyed(*this->key.state(), static_cast<Root<true> *>(this), &Root<true>::declareArm)));
    }
    NodeState<bool> shown;
  };
} // namespace

void testKeyedParkedGenerationKeepsHeapStatesAlive()
{
  AllocationProbe allocations;
  ComponentLifetime lifetime;
  {
    NullScenePlatformController platform;
    Scene scene((BoundaryDefinition<BoundaryPropsFor<Root<true> >, ParkedRoot>((BoundaryPropsFor<Root<true> >()))));
    scene.mount(&platform);
    scene.updateAttached(true);
    ParkedRoot *root = static_cast<ParkedRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    const Snapshot warm(allocations);
    for (int i = 0; i < 4; ++i)
    {
      writeState(*root, root->shown, false);
      scene.flushInvalidation();
      LOKA_VERIFY(lifetime.constructed == 1 && lifetime.destroyed == 0);
      LOKA_VERIFY(Snapshot(allocations) == warm);
      writeState(*root, root->shown, true);
      scene.flushInvalidation();
      LOKA_VERIFY(lifetime.constructed == 1 && lifetime.destroyed == 0);
      LOKA_VERIFY(Snapshot(allocations) == warm);
    }
  }
  verifyBalanced(allocations);
}

void testSectionStorageProviderFlattensAndBoundaryStartsArena()
{
  AllocationProbe allocations;
  {
    StorageBoundary boundary;
    GenerationStateOwner generation;
    const bool generationAttached = generation.attachStateOwner(&boundary, &boundary);
    LOKA_VERIFY(generationAttached);
    BoundarySectionNode outer((SectionProps(1)));
    BoundarySectionNode inner((SectionProps(2)));
    const bool outerAttached = outer.attachStateOwner(&boundary, &generation);
    const bool innerAttached = inner.attachStateOwner(&boundary, &outer);
    LOKA_VERIFY(outerAttached && innerAttached);
    const bool flattened = outer.stateStorageOwner() == &generation && inner.stateStorageOwner() == &generation;
    LOKA_VERIFY(flattened);
    NodeState<int> heap;
    StateBatchBase::CreateImmediateState(&inner, heap, 3);
    const bool heapClassified = heap.state()->isGateAllocated() && !heap.state()->isArenaAllocated();
    LOKA_VERIFY(heapClassified);

    // A nested Boundary terminates the provider route even with a generation
    // as its enclosing logical owner. A Section outside generations uses the
    // identical terminal Boundary provider and batch reservation economy.
    StorageBoundary island;
    const bool islandAttached = island.attachStateOwner(&boundary, &generation);
    LOKA_VERIFY(islandAttached);
    BoundarySectionNode stable((SectionProps(3)));
    const bool stableAttached = stable.attachStateOwner(&island, &island);
    const bool arenaProvider = stable.stateStorageOwner() == &island;
    LOKA_VERIFY(stableAttached && arenaProvider);
    stable.reserveStateArena(16 * StateBatchBase::ArenaBytesForState<int>());
    NodeState<int> states[16];
    for (int i = 0; i < 16; ++i)
    {
      StateBatchBase::CreateStateFromInitial(&stable, states[i], i);
      const bool arenaClassified = states[i].state()->isArenaAllocated() && !states[i].state()->isGateAllocated();
      LOKA_VERIFY(arenaClassified);
    }
    LOKA_VERIFY(allocations.heap.outstanding() == 1);
    LOKA_VERIFY(allocations.slabs.outstanding() == 1);
  }
  verifyBalanced(allocations);
}

namespace
{
  class LocalReconcileRoot : public BoundaryNodeFor<LocalReconcileRoot>
  {
  public:
    explicit LocalReconcileRoot(const BoundaryPropsFor<LocalReconcileRoot> &p)
        : BoundaryNodeFor<LocalReconcileRoot>(p)
    {
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Fragment());
    }
    bool reconcile(NodeDefinitionBase &desired, Scene &scene, IPlatformController &platform)
    {
      this->clearPhaseResults();
      ComponentContext context;
      context.setBoundary(this);
      context.setStateOwner(this);
      context.setScene(&scene);
      context.setPlatformController(&platform);
      return this->reconcileParkedBranch(context, this->compositionRootNode(), &desired);
    }
  };

  class RefusedLocalText : public NodeDefinition<TextProps, TextNode>
  {
  public:
    RefusedLocalText()
        : NodeDefinition<TextProps, TextNode>(TextProps("refused"))
    {
    }
    virtual NodeDefinitionBase *clone() const
    {
      return new RefusedLocalText(*this);
    }
    virtual Node *create() const
    {
      return 0;
    }
    virtual Node *createInPlace(void *) const
    {
      return 0;
    }
  };

  class DescendantRefusalScope;
  struct DescendantRefusalProps : NodePropsBase<DescendantRefusalProps>
  {
    typedef DescendantRefusalScope NodeType;
    bool operator<(const PropsBase &) const
    {
      return false;
    }
  };
  class DescendantRefusalScope : public LazyScopeNode
  {
  public:
    typedef DescendantRefusalProps Props;
    Props props;
    explicit DescendantRefusalScope(const Props &p)
        : props(p)
    {
    }
    virtual void declareScope(NodeComposition &c)
    {
      c.declare(Section(77) << Fragment());
    }
  };
  class DescendantRefusalRoot : public BoundaryNodeFor<DescendantRefusalRoot>
  {
  public:
    explicit DescendantRefusalRoot(const BoundaryPropsFor<DescendantRefusalRoot> &p)
        : BoundaryNodeFor<DescendantRefusalRoot>(p)
    {
      this->state(this->key, 0);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Fragment() << LazyScope(*this->key.state(), DescendantRefusalProps()));
    }
    NodeState<int> key;
  };

  class DirectStateRoot : public BoundaryNodeFor<DirectStateRoot>
  {
  public:
    explicit DirectStateRoot(const BoundaryPropsFor<DirectStateRoot> &p)
        : BoundaryNodeFor<DirectStateRoot>(p),
          first(0),
          second(0),
          declarations(0)
    {
      this->state(this->key, 0);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Fragment() << Keyed(*this->key.state(), this, &DirectStateRoot::declareArm));
    }
    void declareArm(NodeComposition &c)
    {
      ++this->declarations;
      NodeState<int> a, b;
      c.declareStates().state(a, 11).state(b, 22);
      if (a.isValid() && b.isValid())
      {
        this->first = a.state();
        this->second = b.state();
      }
      c.declare(Fragment());
    }
    NodeState<int> key;
    loka::core::State<int> *first, *second;
    int declarations;
  };

  BoundarySectionNode *findSection(Node *node, NodeTag tag)
  {
    if (!node)
      return 0;
    BoundarySectionNode *section = node->asBoundarySectionNode();
    if (section && section->props.key() == tag)
      return section;
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
    {
      BoundarySectionNode *found = findSection(child, tag);
      if (found)
        return found;
    }
    return 0;
  }
} // namespace

void testLocalRebuildSectionMaterializesChildrenWithBoundaryProvider()
{
  AllocationProbe allocations;
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<LocalReconcileRoot>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    LocalReconcileRoot *root =
        static_cast<LocalReconcileRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    Fragment desired;
    desired << (Section(12) << Text("child"));
    const bool reconciled = root->reconcile(desired, scene, platform);
    BoundarySectionNode *section = findSection(root, 12);
    const unsigned children = section ? section->childrenCount() : 0;
    const bool failure = root->composeResult().allocationFailed;
    std::fprintf(stderr, "local Section: reconcile=%d failure=%d children=%u\n", reconciled, failure, children);
    LOKA_VERIFY(reconciled && !failure && children == 1);
    const bool provider = section->stateStorageOwner() == root;
    LOKA_VERIFY(provider);
  }
  verifyBalanced(allocations);
}

void testLocalRebuildRefusalPreservesInstalledSubtree()
{
  AllocationProbe allocations;
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<LocalReconcileRoot>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    LocalReconcileRoot *root =
        static_cast<LocalReconcileRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    Fragment initial;
    initial << Text("old");
    const bool initialized = root->reconcile(initial, scene, platform);
    LOKA_VERIFY(initialized);
    Node *old = root->compositionRootNode()->asNestable()->childrenHead();
    Fragment desired;
    desired << (Fragment() << Text("partial") << RefusedLocalText());
    const bool reconciled = root->reconcile(desired, scene, platform);
    const bool kept = root->compositionRootNode()->asNestable()->childrenHead() == old;
    const bool failure = root->composeResult().allocationFailed;
    std::fprintf(stderr, "local refusal: reconcile=%d failure=%d kept=%d\n", reconciled, failure, kept);
    LOKA_VERIFY(!reconciled && failure && kept);
    scene.flushInvalidation();
    const bool keptAfterDrain = root->compositionRootNode()->asNestable()->childrenHead() == old;
    LOKA_VERIFY(keptAfterDrain);
  }
  verifyBalanced(allocations);
}

void testPublishedLazyScopeSectionRefusalKeepsScopeReady()
{
  AllocationProbe allocations;
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<DescendantRefusalRoot>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    BoundaryNode *root = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
    LazyScopeNode *scope = static_cast<LazyScopeNode *>(findType(root, NodeTypeToken<LazyScopeNode>()));
    BoundarySectionNode *section = findSection(root, 77);
    LOKA_VERIFY(scope && section);
    const bool published = scope->lifecycleFact() == NODE_FACT_ATTACHED;
    LOKA_VERIFY(published);
    const Snapshot before(allocations);
    allocations.heapRequestsBeforeRefusal = 0;
    NodeState<int> refused;
    StateBatchBase::CreateImmediateState(section, refused, 5);
    const bool failure = root->composeResult().allocationFailed;
    std::fprintf(stderr,
                 "published Section refusal: scopeStatus=%d failure=%d refused=%d\n",
                 scope->scopeStatus(),
                 failure,
                 allocations.heapRefusals);
    LOKA_VERIFY(!refused.isValid() && allocations.heapRefusals == 1);
    LOKA_VERIFY(scope->scopeStatus() == LAZY_SCOPE_READY && failure);
    LOKA_VERIFY(Snapshot(allocations) == before);
  }
  verifyBalanced(allocations);
}

void testKeyedDirectDeclarerHeapRefusalRejectsPendingRootAndRetries()
{
  AllocationProbe allocations;
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<DirectStateRoot>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    DirectStateRoot *root = static_cast<DirectStateRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    Node *old = root->compositionRootNode()->asNestable()->childrenHead();
    loka::core::State<int> *oldFirst = root->first;
    const Snapshot before(allocations);
    const int acquired = allocations.heap.allocations, freed = allocations.heap.frees;
    const std::size_t outstandingGates = allocations.live.size();
    LOKA_VERIFY(before.heap == 2);
    allocations.heapRequestsBeforeRefusal = 1;
    writeState(*root, root->key, 1);
    const bool kept = root->compositionRootNode()->asNestable()->childrenHead() == old;
    const bool failure = root->composeResult().allocationFailed;
    LOKA_VERIFY(kept && failure && root->first == oldFirst && oldFirst->get() == 11 && root->second->get() == 22);
    LOKA_VERIFY(allocations.live.size() == outstandingGates);
    LOKA_VERIFY(root->declarations == 2 && allocations.heapRefusals == 1);
    LOKA_VERIFY(allocations.heap.allocations == acquired + 1 && allocations.heap.frees == freed + 1);
    LOKA_VERIFY(Snapshot(allocations) == before);
    {
      loka::core::StateTrackerGuard guard(root->tracker());
      root->key.set(1, true);
    }
    const bool replaced = root->compositionRootNode()->asNestable()->childrenHead() != old;
    LOKA_VERIFY(replaced && root->declarations == 3 && root->first != oldFirst);
    const bool gate = root->first->isGateAllocated() && !root->first->isArenaAllocated()
                      && root->second->isGateAllocated() && !root->second->isArenaAllocated();
    LOKA_VERIFY(gate && root->first->get() == 11 && root->second->get() == 22);
    scene.flushInvalidation();
    LOKA_VERIFY(Snapshot(allocations) == before);
    std::printf("direct declarer refusal: rejected=1 retry=1 outstanding=%d rejected-state-frees=1\n", before.heap);
  }
  verifyBalanced(allocations);
}

void testNestedKeyedOuterThenInnerBeforeDrainPreservesProviders()
{
  AllocationProbe allocations;
  ComponentLifetime lifetime;
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<NestedRoot>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    NestedRoot *root = static_cast<NestedRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    const Snapshot before(allocations);
    BoundarySectionNode *oldOuter = findSection(root, 6401), *oldInner = findSection(root, 6404);
    IStateOwner *oldOuterProvider = oldOuter->stateStorageOwner(), *oldInnerProvider = oldInner->stateStorageOwner();
    loka::core::State<int> *oldState = static_cast<ResidentNode *>(oldInner->childrenHead())->valueState();
    replaceWithoutDrain(scene, *root, root->outer, 1);
    BoundarySectionNode *newOuter = findSection(root, 6401), *retiringInner = findSection(root, 6404);
    IStateOwner *newOuterProvider = newOuter->stateStorageOwner(),
                *retiringProvider = retiringInner->stateStorageOwner();
    loka::core::State<int> *retiringState = static_cast<ResidentNode *>(retiringInner->childrenHead())->valueState();
    LOKA_VERIFY(newOuterProvider != oldOuterProvider && retiringProvider != oldInnerProvider);
    LOKA_VERIFY(allocations.heap.outstanding() == 4 && lifetime.destroyed == 0);
    replaceWithoutDrain(scene, *root, root->inner, 1);
    BoundarySectionNode *current = findSection(root, 6404);
    IStateOwner *currentProvider = current->stateStorageOwner();
    loka::core::State<int> *currentState = static_cast<ResidentNode *>(current->childrenHead())->valueState();
    const bool providersAlive =
        oldOuter->stateStorageOwner() == oldOuterProvider && oldInner->stateStorageOwner() == oldInnerProvider
        && oldOuterProvider->stateStorageOwner() == oldOuterProvider
        && oldInnerProvider->stateStorageOwner() == oldInnerProvider
        && retiringInner->stateStorageOwner() == retiringProvider
        && retiringProvider->stateStorageOwner() == retiringProvider
        && newOuter->stateStorageOwner() == newOuterProvider && currentProvider != retiringProvider;
    LOKA_VERIFY(providersAlive);
    LOKA_VERIFY(oldState->get() == 7 && retiringState->get() == 7 && currentState->get() == 7);
    const bool heap = oldState->isGateAllocated() && !oldState->isArenaAllocated() && retiringState->isGateAllocated()
                      && !retiringState->isArenaAllocated() && currentState->isGateAllocated()
                      && !currentState->isArenaAllocated();
    LOKA_VERIFY(heap && allocations.heap.outstanding() == 5 && lifetime.destroyed == 0);
    scene.flushInvalidation();
    LOKA_VERIFY(lifetime.constructed == 5 && lifetime.destroyed == 3);
    const bool currentAlive = current->stateStorageOwner() == currentProvider
                              && currentProvider->stateStorageOwner() == currentProvider && currentState->get() == 7;
    LOKA_VERIFY(currentAlive && Snapshot(allocations) == before);
    std::printf("outer-then-inner: heap=2/4/5/2 destroyed-after-drain=3\n");
  }
  LOKA_VERIFY(lifetime.constructed == lifetime.destroyed);
  verifyBalanced(allocations);
}
