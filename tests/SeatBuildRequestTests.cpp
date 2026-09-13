#include "SeatBuildRequestTests.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/nodes/nestable/Keyed.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "testing/core/HeldTestAccess.hpp"
#include <cstdlib>

using namespace loka::app;
using namespace loka::app::scene;
using namespace loka::app::scene::detail;

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class SeatBuildRequestAccess
      {
      public:
        static const SeatReservation *firstReservation(BoundaryNode &owner)
        {
          return owner.branchSeats_.plans()[0].seat()->seatReservation();
        }
        static const SeatReservation *nestedReservation(BoundaryNode &owner)
        {
          return owner.branchSeats_.plans()[0].seat()->declaredBranchSeats()->plans()[0].seat()->seatReservation();
        }
        static unsigned scopeReferences(const SeatReservation &seat)
        {
          return (seat.request().source_ != 0 ? 1u : 0u) + (seat.request().position_.parent != 0 ? 1u : 0u);
        }
        static bool remove(BoundaryNode &owner, const SeatReservation &seat, Node *node, int order)
        {
          return owner.seatReservations_.removeSeatChild(seat.request(), &owner, node, order);
        }
        static bool install(BoundaryNode &owner, const SeatReservation &seat, Node *node)
        {
          return owner.seatReservations_.installSeatChild(seat.request(), node);
        }
        static void park(BoundaryNode &owner, Node *node)
        {
          owner.parkBranch(BoundaryParkedBranchKey(9002, 0, 0, 0), node, 0);
        }
      };
    } // namespace testing
  } // namespace dsl
} // namespace loka

namespace
{
  void keyedWaitingScenario(int latestKey);
  void nestedCancellationScenario();
  void vacantSeatPositions();
  void require(bool result)
  {
    if (!result)
      std::abort();
  }
  struct Owner : BoundaryNode
  {
    using BoundaryNode::retireDetachedNode;
    using BoundaryNode::retireOwnedNodeGeneration;
    virtual void composeWithContext(ComponentContext &, ComposeEvent) {}
  };
  struct Counts
  {
    Counts()
        : builds(0),
          deaths(0),
          serial(0),
          key(0),
          model(0)
    {
    }
    int builds, deaths, serial, key, model;
  };
  struct Resident : NestableNode
  {
    explicit Resident(Counts &c)
        : counts(c),
          identity(++c.serial)
    {
    }
    ~Resident()
    {
      ++this->counts.deaths;
    }
    Counts &counts;
    const int identity;
  };
  SeatLayoutTable table(size_t n)
  {
    SeatLayoutTable result;
    require(result.append(NodeSlotLayout::of<Resident>(n)));
    require(result.normalize());
    return result;
  }
  struct Build : NodeBuildOperation
  {
    struct Factory
    {
      explicit Factory(Counts &c)
          : counts(c)
      {
      }
      Resident *construct(void *storage)
      {
        return new (storage) Resident(this->counts);
      }
      Counts &counts;
    } factory;
    Build(Counts &c, const int &k, const int &m, size_t n = 2, bool link = true)
        : factory(c),
          key(k),
          model(m),
          count(n),
          linked(link),
          root(0)
    {
    }
    bool buildAndAttach(NodeBuildTicket &ticket)
    {
      ++this->factory.counts.builds;
      this->factory.counts.key = this->key;
      this->factory.counts.model = this->model;
      this->root = ticket.create<Resident>(this->factory);
      if (!this->root)
        return false;
      for (size_t i = 1; i < this->count; ++i)
      {
        Resident *child = ticket.create<Resident>(this->factory, this->root);
        if (!child)
          return false;
        if (this->linked)
          this->root->addChild(child);
      }
      return true;
    }
    const int &key, &model;
    size_t count;
    bool linked;
    Resident *root;
  };
  const SeatReservation &request(Owner &owner, NodePartition &bank, size_t count)
  {
    const SeatReservation *reservation = owner.installSeatReservation(table(count));
    require(reservation != 0);
    reservation->request().activateFixture(bank);
    return *reservation;
  }
  void retire(Owner &owner, Node *node)
  {
    ComponentContext context;
    context.setBoundary(&owner);
    owner.retireDetachedNode(context, node);
  }
  void replace(Owner &owner, const SeatReservation &seat, Node *node)
  {
    seat.request().retire(node);
    retire(owner, node);
  }
} // namespace

void testSeatBuildRequestResamplesLatestBoard()
{
  keyedWaitingScenario(2);
  Counts counts;
  Owner owner;
  NodePartition *bank = owner.installPartitionFixture(table(2));
  require(bank != 0);
  const SeatReservation &seat = request(owner, *bank, 2);
  int key = 0, board = 0;
  Build build(counts, key, board);
  require(bank->buildFixture(seat.layoutTable().layouts(), seat.layoutTable().count(), build));
  replace(owner, seat, build.root);
  key = board = 1;
  seat.request().mark();
  require(!seat.request().admit(seat.layoutTable(), build));
  key = board = 2;
  seat.request().mark();
  require(!seat.request().admit(seat.layoutTable(), build));
  assert(counts.builds == 1 && counts.deaths == 0);
  owner.drainRetiredSubtreesAtNextTrackerRun();
  require(seat.request().admit(seat.layoutTable(), build));
  assert(counts.builds == 2 && counts.key == 2 && counts.model == 2);
  assert(!seat.request().waiting());
}

void testSeatBuildRequestFreshReturnToA()
{
  keyedWaitingScenario(0);
  Counts counts;
  Owner owner;
  NodePartition *bank = owner.installPartitionFixture(table(4));
  require(bank != 0);
  const SeatReservation &seat = request(owner, *bank, 2);
  int key = 0, model = 0;
  Build build(counts, key, model);
  require(bank->buildFixture(seat.layoutTable().layouts(), seat.layoutTable().count(), build));
  const int oldIdentity = build.root->identity;
  (void)oldIdentity;
  replace(owner, seat, build.root);
  key = 1;
  require(!seat.request().admit(seat.layoutTable(), build));
  key = 0;
  seat.request().mark();
  // Spare capacity cannot authorize a second incarnation of retiring A.
  require(!seat.request().admit(seat.layoutTable(), build));
  assert(counts.builds == 1 && counts.deaths == 0);
  owner.drainRetiredSubtreesAtNextTrackerRun();
  require(seat.request().admit(seat.layoutTable(), build));
  assert(counts.builds == 2 && counts.key == 0 && counts.deaths == 2);
  assert(build.root->identity != oldIdentity);
}

void testSeatBuildRequestWaitsForFullReturn()
{
  keyedWaitingScenario(1);
  for (int bounded = 0; bounded != 2; ++bounded)
  {
    Counts counts;
    Owner owner;
    NodePartition *bank = owner.installPartitionFixture(table(2));
    require(bank != 0);
    if (bounded)
    {
      require(bank->reserveReclaimScratch(4));
      require(owner.nodeArena()->reserveReclaimScratch(4));
    }
    const SeatReservation &seat = request(owner, *bank, 2);
    int key = 0, model = 0;
    Build build(counts, key, model);
    require(bank->buildFixture(seat.layoutTable().layouts(), seat.layoutTable().count(), build));
    replace(owner, seat, build.root);
    ++key;
    require(!seat.request().admit(seat.layoutTable(), build));
    assert(seat.request().retiring() && counts.builds == 1);
    owner.drainRetiredSubtreesAtNextTrackerRun();
    assert(!seat.request().retiring() && counts.deaths == 2);
    assert(counts.builds == 1);
    require(seat.request().admit(seat.layoutTable(), build));
    assert(counts.builds == 2 && counts.key == 1);
    const SeatReservation &legacy = request(owner, *bank, 1);
    owner.nodeArena()->reserve(sizeof(Resident));
    void *storage = owner.nodeArena()->allocate(sizeof(Resident), AlignOf<Resident>::value);
    require(storage != 0);
    Resident *legacyRoot = new (storage) Resident(counts);
    owner.nodeArena()->registerNode(legacyRoot);
    replace(owner, legacy, legacyRoot);
    owner.retireOwnedNodeGeneration();
    assert(legacy.request().retiring());
    owner.drainRetiredSubtreesAtNextTrackerRun();
    assert(!legacy.request().retiring());
  }
}

void testSeatBuildRequestCancelAndAddressReuse()
{
  nestedCancellationScenario();
  Counts counts;
  Owner owner;
  NodePartition *bank = owner.installPartitionFixture(table(2));
  require(bank != 0);
  const SeatReservation &old = request(owner, *bank, 2);
  int key = 0, model = 0;
  Build build(counts, key, model);
  require(bank->buildFixture(old.layoutTable().layouts(), old.layoutTable().count(), build));
  const void *oldAddress = build.root;
  (void)oldAddress;
  replace(owner, old, build.root);
  old.request().cancel();
  old.request().mark();
  assert(!old.request().waiting() && old.request().retiring());
  owner.drainRetiredSubtreesAtNextTrackerRun();
  assert(!old.request().retiring() && counts.deaths == 2);
  const SeatReservation &fresh = request(owner, *bank, 2);
  fresh.request().mark();
  require(fresh.request().admit(fresh.layoutTable(), build));
  assert(build.root == oldAddress);
  require(!old.request().admit(old.layoutTable(), build));
  assert(counts.builds == 2 && !old.request().waiting());
}

void testSeatBuildRequestParkedAndNestedOccupancy()
{
  Counts counts;
  Owner owner;
  NodePartition *bank = owner.installPartitionFixture(table(2));
  require(bank != 0);
  int key = 0, model = 0;
  Build parked(counts, key, model, 1);
  const SeatLayoutTable one = table(1);
  require(bank->buildFixture(one.layouts(), one.count(), parked));
  NotifySubtreeNodeDetached(parked.root);
  loka::dsl::testing::SeatBuildRequestAccess::park(owner, parked.root);
  const SeatReservation &seat = request(owner, *bank, 2);
  seat.request().mark();
  Build incoming(counts, key, model);
  require(!seat.request().admit(seat.layoutTable(), incoming));
  assert(counts.builds == 1 && counts.deaths == 0 && !seat.request().retiring());
  // A nested landlord has both a detached obligation and an unconnected resident.
  Owner *nested = new Owner;
  NodePartition *inner = nested->installPartitionFixture(table(2));
  require(inner != 0);
  Build innerBuild(counts, key, model, 2, false);
  require(inner->buildFixture(seat.layoutTable().layouts(), seat.layoutTable().count(), innerBuild));
  retire(*nested, innerBuild.root);
  const SeatReservation &outer = request(owner, *bank, 1);
  replace(owner, outer, nested);
  Build oneBuild(counts, key, model, 1);
  require(!outer.request().admit(outer.layoutTable(), oneBuild));
  owner.drainRetiredSubtreesAtNextTrackerRun();
  assert(counts.deaths == 2 && !outer.request().retiring());
  require(outer.request().admit(outer.layoutTable(), oneBuild));
  assert(counts.builds == 3);
}

void testSeatBuildRequestSerialConsumption()
{
  vacantSeatPositions();
  Counts counts;
  Owner owner;
  NodePartition *bank = owner.installPartitionFixture(table(2));
  require(bank != 0);
  const SeatReservation &first = request(owner, *bank, 2);
  const SeatReservation &second = request(owner, *bank, 2);
  first.request().mark();
  second.request().mark();
  int key = 0, model = 0;
  Build build(counts, key, model);
  require(first.request().admit(first.layoutTable(), build));
  require(!second.request().admit(second.layoutTable(), build));
  assert(counts.builds == 1 && second.request().waiting());
  retire(owner, build.root);
  owner.drainRetiredSubtreesAtNextTrackerRun();
  require(second.request().admit(second.layoutTable(), build));
  assert(counts.builds == 2);
}

namespace
{
  void vacantSeatPositions()
  {
    Counts counts;
    Owner owner;
    NodePartition *bank = owner.installPartitionFixture(table(4));
    require(bank != 0);
    const SeatReservation &first = request(owner, *bank, 2);
    const SeatReservation &second = request(owner, *bank, 2);
    int key = 0, model = 0;
    Build a(counts, key, model), b(counts, key, model);
    require(bank->buildFixture(first.layoutTable().layouts(), first.layoutTable().count(), a));
    require(bank->buildFixture(second.layoutTable().layouts(), second.layoutTable().count(), b));
    owner.addChild(a.root);
    owner.addChild(b.root);
    Resident *tail = new Resident(counts);
    owner.addChild(tail);
    typedef loka::dsl::testing::SeatBuildRequestAccess Access;
    require(Access::remove(owner, first, a.root, 0));
    retire(owner, a.root);
    require(Access::remove(owner, second, b.root, 1));
    retire(owner, b.root);
    assert(owner.childrenHead() == tail && owner.childrenCount() == 1); // loka-assert-ok: child-list getters only read existing links/counts.
    owner.drainRetiredSubtreesAtNextTrackerRun();
    // An earlier ineligible row need not block a later one. Installing the
    // later seat first still preserves logical order around the surviving tail.
    require(second.request().admit(second.layoutTable(), b));
    require(Access::install(owner, second, b.root));
    require(first.request().admit(first.layoutTable(), a));
    require(Access::install(owner, first, a.root));
    assert(owner.childrenHead() == a.root && a.root->nextInComposition == b.root); // loka-assert-ok: child-list getters only read existing links/counts.
    assert(b.root->nextInComposition == tail && owner.childrenCount() == 3); // loka-assert-ok: child-list getters only read existing links/counts.
  }

  struct KeyedOwner : BoundaryNodeFor<KeyedOwner>
  {
    explicit KeyedOwner(const BoundaryPropsFor<KeyedOwner> &p)
        : BoundaryNodeFor<KeyedOwner>(p),
          builds(0),
          sampled(-1),
          model(0),
          sampledModel(-1)
    {
      this->state(this->key, 0);
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
    void composeNode(NodeComposition &c)
    {
      c.declare(Fragment() << Keyed(*this->key.state(),
                                    this,
                                    &KeyedOwner::arm,
                                    reservation::SeatNodes<reservation::Nodes<FragmentNode, 1, reservation::End> >()));
    }
    void arm(NodeComposition &c)
    {
      ++this->builds;
      this->sampled = this->key.get();
      this->sampledModel = this->model;
      c.declare(Fragment());
    }
    NodeState<int> key;
    int builds, sampled, model, sampledModel;
  };
} // namespace

namespace
{
  void keyedWaitingScenario(int latestKey)
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<KeyedOwner>()));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    KeyedOwner *owner = static_cast<KeyedOwner *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    require(owner != 0);
    const SeatReservation *seat = loka::dsl::testing::SeatBuildRequestAccess::firstReservation(*owner);
    require(seat != 0);
    require(seat->request().enabled());
    {
      loka::core::StateTrackerGuard guard(owner->tracker());
      owner->model = 1;
      owner->key.set(1);
    }
    scene.flushInvalidation();
    assert(owner->builds == 1 && seat->request().waiting() && seat->request().retiring());
    assert(scene.hasPendingInvalidation());
    if (latestKey != 1)
    {
      loka::core::StateTrackerGuard guard(owner->tracker());
      owner->model = latestKey;
      owner->key.set(latestKey);
    }
    scene.flushInvalidation();
    assert(owner->builds == 2 && owner->sampled == latestKey && !seat->request().waiting());
    assert(owner->sampledModel == latestKey);
    (void)latestKey;
  }

  struct NestedKeyedOwner : BoundaryNodeFor<NestedKeyedOwner>
  {
    explicit NestedKeyedOwner(const BoundaryPropsFor<NestedKeyedOwner> &p)
        : BoundaryNodeFor<NestedKeyedOwner>(p),
          builds(0),
          childBuilds(0)
    {
      this->state(this->outer, 0);
      this->state(this->inner, 0);
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
    void composeNode(NodeComposition &c)
    {
      c.declare(Fragment() << Keyed(*this->outer.state(),
                                    this,
                                    &NestedKeyedOwner::arm,
                                    reservation::SeatNodes<reservation::Nodes<FragmentNode, 1, reservation::End> >()));
    }
    void arm(NodeComposition &c)
    {
      ++this->builds;
      c.declare(Fragment() << Keyed(*this->inner.state(),
                                    this,
                                    &NestedKeyedOwner::childArm,
                                    reservation::SeatNodes<reservation::Nodes<FragmentNode, 1, reservation::End> >()));
    }
    void childArm(NodeComposition &c)
    {
      ++this->childBuilds;
      c.declare(Fragment());
    }
    NodeState<int> outer, inner;
    int builds, childBuilds;
  };
  void nestedCancellationScenario()
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<NestedKeyedOwner>()));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    NestedKeyedOwner *owner = static_cast<NestedKeyedOwner *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    require(owner != 0);
    typedef loka::dsl::testing::SeatBuildRequestAccess Access;
    const SeatReservation *outer = Access::firstReservation(*owner);
    const SeatReservation *inner = Access::nestedReservation(*owner);
    require(outer != 0 && inner != 0);
    require(outer->request().enabled() && inner->request().enabled());
    {
      loka::core::StateTrackerGuard guard(owner->tracker());
      owner->inner.set(1);
    }
    scene.flushInvalidation();
    assert(owner->childBuilds == 1 && inner->request().waiting());
    {
      loka::core::StateTrackerGuard guard(owner->tracker());
      owner->outer.set(1);
    }
    scene.flushInvalidation();
    assert(owner->childBuilds == 1 && !inner->request().waiting());
    assert(Access::scopeReferences(*inner) == 0);
    assert(!inner->request().retiring() && outer->request().retiring());
    scene.flushInvalidation();
    assert(owner->builds == 2 && owner->childBuilds == 2);
    inner->request().mark();
    assert(!inner->request().waiting());
    assert(Access::nestedReservation(*owner) != inner);
  }
} // namespace

void testSeatBuildRequestKeyedNoEventAdmission()
{
  keyedWaitingScenario(1);
}

namespace
{
  struct AttachHoldScenario
  {
    AttachHoldScenario()
        : creator(0),
          attached(0),
          attaches(0)
    {
    }
    loka::core::Held<int> held;
    IStateOwner *creator;
    IStateOwner *attached;
    int attaches;
  };
  AttachHoldScenario *attachHoldScenario = 0;
  void releaseAttachPayload(int *value)
  {
    delete value;
  }
  class AttachHoldNode;
  struct AttachHoldTag
  {
  };
  struct AttachHoldProps : NodePropsBase<AttachHoldProps>
  {
    typedef AttachHoldTag TypeTag;
    typedef AttachHoldNode NodeType;
    bool operator<(const PropsBase &) const
    {
      return false;
    }
  };
  class AttachHoldNode : public ComponentNodeWithProps<AttachHoldProps>
  {
  public:
    explicit AttachHoldNode(const AttachHoldProps &p)
        : ComponentNodeWithProps<AttachHoldProps>(p)
    {
    }

  protected:
    virtual void composeChildren(NodeComposition &) {}
    virtual void attachNode(NodeComposition &c)
    {
      if (this->acquired_.isValid())
        return;
      this->acquired_ = c.hold(attachHoldScenario->held);
      require(this->acquired_.isValid());
      attachHoldScenario->attached = c.componentContext()->stateOwner();
      ++attachHoldScenario->attaches;
    }
  private:
    loka::core::Held<int> acquired_;
  };
  struct AttachHoldOwner : BoundaryNodeFor<AttachHoldOwner>
  {
    explicit AttachHoldOwner(const BoundaryPropsFor<AttachHoldOwner> &p)
        : BoundaryNodeFor<AttachHoldOwner>(p),
          builds(0)
    {
      this->state(this->outer, 0);
      this->state(this->inner, 0);
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
    void composeNode(NodeComposition &c)
    {
      c.declare(Fragment() << Keyed(*this->outer.state(),
                                    this,
                                    &AttachHoldOwner::arm,
                                    reservation::SeatNodes<reservation::Nodes<FragmentNode, 1, reservation::End> >()));
    }
    void arm(NodeComposition &c)
    {
      attachHoldScenario->creator = c.componentContext()->stateOwner();
      attachHoldScenario->held = c.hold(new int(7), &releaseAttachPayload);
      c.declare(
          Fragment() << Keyed(*this->inner.state(),
                              this,
                              &AttachHoldOwner::childArm,
                              reservation::SeatNodes<reservation::Nodes<AttachHoldNode, 1, reservation::End> >()));
    }
    void childArm(NodeComposition &c)
    {
      ++this->builds;
      c.declare(NodeDefinition<AttachHoldProps, AttachHoldNode>());
    }
    NodeState<int> outer, inner;
    int builds;
  };
} // namespace

void testSeatBuildRequestNestedAttachPreservesHeldOwner()
{
  AttachHoldScenario scenario;
  attachHoldScenario = &scenario;
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<AttachHoldOwner>()));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    AttachHoldOwner *owner = static_cast<AttachHoldOwner *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    require(owner != 0);
    typedef loka::core::testing::HeldTestAccess HeldAccess;
    // Both replacements now use production waiting admission.
    {
      loka::core::StateTrackerGuard guard(owner->tracker());
      owner->inner.set(1);
    }
    scene.flushInvalidation();
    scene.flushInvalidation();
    assert(owner->builds == 2 && scenario.attaches == 2);
    IStateOwner *ordinaryParent = HeldAccess::enclosingOwner(*scenario.attached->holdLedger());
    assert(ordinaryParent == scenario.creator);
    const SeatReservation *seat = loka::dsl::testing::SeatBuildRequestAccess::nestedReservation(*owner);
    require(seat != 0);
    require(seat->request().enabled());
    {
      loka::core::StateTrackerGuard guard(owner->tracker());
      owner->inner.set(2);
    }
    scene.flushInvalidation();
    assert(owner->builds == 2 && seat->request().waiting());
    scene.flushInvalidation();
    assert(owner->builds == 3 && scenario.attaches == 3 && !seat->request().waiting());
    assert(HeldAccess::enclosingOwner(*scenario.attached->holdLedger()) == ordinaryParent);
    assert(HeldAccess::holdCountForOwner(scenario.held, scenario.attached) == 1);
    (void)ordinaryParent;
  }
  attachHoldScenario = 0;
}
