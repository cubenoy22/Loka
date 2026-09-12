#include "testing/scene/SceneTestFlow.hpp"
#include "LazyScopeTests.hpp"
#include "support/TestVerify.hpp"
#include "app/scene/boundary/LazyScopeDefinition.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/OwnershipDump.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include <cstring>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  class ProbeScopeNode;
  struct Record
  {
    Record()
        : current(0),
          constructed(0),
          destroyed(0),
          declared(0),
          updates(0),
          failed(LAZY_SCOPE_READY),
          crossing(false),
          outer(false),
          externalSource(0),
          copyCalls(0)
    {
      for (int i = 0; i < 4; ++i)
        texts[i] = 0;
    }
    ProbeScopeNode *current;
    int constructed, destroyed, declared, updates;
    int texts[4];
    LazyScopeStatus failed;
    bool crossing, outer;
    loka::core::State<int> *externalSource;
    int copyCalls;
  };
  Record *record = 0;
  class CountedText : public TextNode
  {
  public:
    typedef TextTypeTag TypeTag;
    explicit CountedText(const TextProps &p)
        : TextNode(p)
    {
      ++record->texts[2];
    }
  };
  struct ScopeTag
  {
  };
  struct ScopeProps : NodePropsBase<ScopeProps>
  {
    typedef ScopeTag TypeTag;
    typedef ProbeScopeNode NodeType;
    explicit ScopeProps(loka::core::State<int> *s = 0)
        : source(s)
    {
    }
    bool operator<(const PropsBase &) const
    {
      return false;
    }
    loka::core::State<int> *source;
  };
  struct Positive : loka::core::DerivedState<bool>::EvalFn
  {
    explicit Positive(loka::core::State<int> *s)
        : source(s)
    {
    }
    bool operator()()
    {
      return this->source->get() >= 0;
    }
    loka::core::State<int> *source;
  };
  class ProbeScopeNode : public LazyScopeNode
  {
  public:
    typedef ScopeProps Props;
    typedef ScopeTag TypeTag;
    Props props;
    explicit ProbeScopeNode(const Props &p)
        : props(p),
          copy_(),
          positive_(0)
    {
      ++record->constructed;
      this->declareStates(5)
          .state(this->visible_[0], true)
          .state(this->visible_[1], true)
          .state(this->visible_[2], true)
          .state(this->visible_[3], true)
          .state(this->copy_, 0);
    }
    virtual ~ProbeScopeNode()
    {
      ++record->destroyed;
      if (this->scopeStatus() != LAZY_SCOPE_READY)
        record->failed = this->scopeStatus();
    }
    void show(int i, bool value)
    {
      loka::core::StateTrackerGuard guard(this->asStateOwner()->tracker());
      this->visible_[i].set(value);
    }
    int copy() const
    {
      return this->copy_.get();
    }
    virtual void declareBindings(BindingToken &token)
    {
      token.watch(*this->props.source, this, &ProbeScopeNode::copySource, true);
      if (record->crossing && !this->positive_)
      {
        this->positive_ = new loka::core::DerivedState<bool>(this->copy_.state(), new Positive(this->copy_.state()));
        this->asStateOwner()->adoptState(this->positive_);
      }
    }
    void copySource()
    {
      ++record->copyCalls;
      loka::core::StateTrackerGuard guard(this->asStateOwner()->tracker());
      this->copy_.set(this->props.source->get());
    }
    virtual void declareScope(NodeComposition &c)
    {
      {
        const bool verified = (NodeComposition::current() == &c);
        LOKA_VERIFY(verified);
      }
      {
        const bool verified = (c.componentContext()->owner() == this);
        LOKA_VERIFY(verified);
      }
      {
        const bool verified = (c.componentContext()->stateOwner() == this->asStateOwner());
        LOKA_VERIFY(verified);
      }
      for (int i = 0; i < 4; ++i)
      {
        const bool verified = (this->visible_[i].isValid());
        LOKA_VERIFY(verified);
      }
      {
        const bool verified = (this->copy_.isValid());
        LOKA_VERIFY(verified);
      }
      record->current = this;
      ++record->declared;
      ColumnDefinition column;
      for (int i = 0; i < 4; ++i)
      {
        if (i == 2)
          column << (Show(*this->visible_[i].state()).destroyOnDetach()
                     << (Fragment() << NodeDefinition<TextProps, CountedText>(TextProps("2")) << Button("row")));
        else
          column << (Show(*this->visible_[i].state()).destroyOnDetach()
                     << (Fragment() << Text("row") << Button("row")));
      }
      if (record->crossing)
      {
        LOKA_VERIFY(this->positive_ != 0);
        column << (Show(*this->positive_).destroyOnDetach() << Button("derived"));
      }
      c.declare(column);
    }

  private:
    NodeState<bool> visible_[4];
    NodeState<int> copy_;
    loka::core::State<bool> *positive_;
  };
  class Root : public BoundaryNodeFor<Root>
  {
  public:
    explicit Root(const BoundaryPropsFor<Root> &p)
        : BoundaryNodeFor<Root>(p)
    {
      this->state(this->key, 1);
      this->state(this->source, 0);
      this->state(this->shown, true);
    }
    virtual void composeNode(NodeComposition &c)
    {
      if (record->outer)
        c.declare(
            Fragment() << (Show(*this->shown.state()).destroyOnDetach() << LazyScope(
                               *this->key.state(),
                               ScopeProps(record->externalSource ? record->externalSource : this->source.state()))));
      else
        c.declare(Fragment() << LazyScope(
                      *this->key.state(),
                      ScopeProps(record->externalSource ? record->externalSource : this->source.state())));
    }
    virtual void composeWithContext(ComponentContext &c, ComposeEvent e)
    {
      if (e == COMPOSE_EVENT_UPDATE)
        ++record->updates;
      BoundaryNodeFor<Root>::composeWithContext(c, e);
    }
    IBranchSeatDefinition *seat()
    {
      return this->composition().root()->asNestableDefinition()->childrenHead()->asBranchSeatDefinition();
    }
    NodeState<int> key, source;
    NodeState<bool> shown;
  };
  Root *root(Scene &scene)
  {
    return static_cast<Root *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  }
  void writePendingSource(void *data)
  {
    loka::core::MutableState<int> *source = static_cast<loka::core::MutableState<int> *>(data);
    loka::core::StateTrackerGuard guard(source->trackerOwner());
    source->set(9);
  }
  bool refuse = false;
  int refusals = 0;
  void *allocate(std::size_t size, const loka::core::LokaAllocationSite &site)
  {
    if (refuse && (std::strcmp(site.ownerTag, "StateArena") == 0 || std::strcmp(site.ownerTag, "StateOwner") == 0))
    {
      ++refusals;
      return 0;
    }
    return new (std::nothrow) char[size];
  }
  void freeAllocation(void *ptr, const loka::core::LokaAllocationSite &)
  {
    delete[] static_cast<char *>(ptr);
  }
} // namespace

void testLazyScopeMaterializesOwnedDeclaration()
{
  Record r;
  record = &r;
  NullScenePlatformController platform;
  Scene scene((Boundary<Root>()));
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  {
    const bool verified = (r.constructed == 1 && r.declared == 1);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (root(scene)->seat()->declaredBranchSeats()->plans().size() == 4);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (platform.ledger().size() == 4);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (r.current->scopeStatus() == LAZY_SCOPE_READY);
    LOKA_VERIFY(verified);
  }
  const std::string rows = loka::dsl::testing::OwnershipDump::dumpSeatRuntime(*root(scene));
  size_t count = 0;
  for (size_t i = 0; i < rows.size(); ++i)
    if (rows[i] == '\n')
      ++count;
  LOKA_VERIFY(count == 5); // One LazyScope row plus four Show rows in the Boundary ledger.
  const std::string owners = loka::dsl::testing::OwnershipDump::dump(scene);
  const bool fiveStates = owners.find("states: 5 (arena 0, heap 5)") != std::string::npos;
  LOKA_VERIFY(fiveStates);
}
void testLazyScopeOwnTrackerAppliesShow()
{
  Record r;
  record = &r;
  NullScenePlatformController platform;
  Scene scene((Boundary<Root>()));
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  int originalHandles[4];
  for (int i = 0; i < 4; ++i)
    originalHandles[i] = platform.ledger()[i].handle->id;
  r.current->show(2, false);
  scene.flushInvalidation();
  platform.drainNativeRetirements();
  {
    const bool verified = (platform.ledger().size() == 3);
    LOKA_VERIFY(verified);
  }
  for (int i = 0; i < 3; ++i)
  {
    const bool sameSurvivor = platform.ledger()[i].handle->id == originalHandles[i < 2 ? i : i + 1];
    LOKA_VERIFY(sameSurvivor);
  }
  {
    const bool verified = (root(scene)->parkedBranchCountForTesting() == 0);
    LOKA_VERIFY(verified);
  }
  r.current->show(2, true);
  {
    const bool verified = (platform.ledger().size() == 4 && r.texts[2] == 2);
    LOKA_VERIFY(verified);
  }
}
void testLazyScopeCrossesTrackersInOneUpdate()
{
  Record r;
  record = &r;
  r.crossing = true;
  NullScenePlatformController platform;
  Scene scene((Boundary<Root>()));
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  const int before = r.updates;
  root(scene)->source.set(-1);
  scene.flushInvalidation();
  platform.drainNativeRetirements();
  {
    const bool verified = (r.current->copy() == -1);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (platform.ledger().size() == 4);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (r.updates == before + 1);
    LOKA_VERIFY(verified);
  }
}
void testLazyScopeKeyReplacementRetiresOwner()
{
  Record r;
  record = &r;
  NullScenePlatformController platform;
  Scene scene((Boundary<Root>()));
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  ProbeScopeNode *old = r.current;
  root(scene)->key.set(2);
  {
    const bool verified = (r.constructed == 2 && r.declared == 2 && r.current != old);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (old->lifecycleFact() == NODE_FACT_RETIRED);
    LOKA_VERIFY(verified);
  }
  scene.flushInvalidation();
  platform.drainNativeRetirements();
  {
    const bool verified = (platform.ledger().size() == 4);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (r.destroyed == 1);
    LOKA_VERIFY(verified);
  }
  const std::string owners = loka::dsl::testing::OwnershipDump::dump(scene);
  const size_t owner = owners.find("lazy-scope");
  const bool oneOwner = owner != std::string::npos && owners.find("lazy-scope", owner + 1) == std::string::npos;
  LOKA_VERIFY(oneOwner);
  printf("LazyScope after replacement and drain:\n%s", owners.c_str());
  {
    const bool verified = (root(scene)->seat()->declaredBranchSeats()->plans().size() == 4);
    LOKA_VERIFY(verified);
  }
  // Generation storage never touches the Boundary's bump-only StateArena:
  // ten more replacements leave the Boundary's arena rows unchanged and every
  // lazy-scope state on the heap, so nothing accumulates across generations.
  for (int key = 3; key <= 12; ++key)
  {
    root(scene)->key.set(key);
    scene.flushInvalidation();
    platform.drainNativeRetirements();
  }
  const std::string after = loka::dsl::testing::OwnershipDump::dump(scene);
  {
    const bool verified = (r.constructed == 12 && r.destroyed == 11);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (after.find("states: 5 (arena 0, heap 5)") != std::string::npos);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (after.find("states: 3 (arena 3, heap 0)") != std::string::npos && after.find("(arena 3, heap 0)") == after.rfind("(arena 3, heap 0)"));
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (platform.ledger().size() == 4);
    LOKA_VERIFY(verified);
  }
}
void testLazyScopeStateRefusalPreservesArm()
{
  Record r;
  record = &r;
  refusals = 0;
  loka::core::LokaAllocSetBackend(&allocate, &freeAllocation);
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<Root>()));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    ProbeScopeNode *old = r.current;
    refuse = true;
    root(scene)->key.set(2);
    refuse = false;
    {
      const bool verified = (refusals > 0);
      LOKA_VERIFY(verified);
    }
    {
      const bool verified = (r.failed == LAZY_SCOPE_STATES_REFUSED && r.destroyed == 1);
      LOKA_VERIFY(verified);
    }
    {
      const bool verified = (r.current == old && r.declared == 1 && platform.ledger().size() == 4);
      LOKA_VERIFY(verified);
    }
    {
      const bool verified = (root(scene)->seat()->needsBranchDeclaration());
      LOKA_VERIFY(verified);
    }
    root(scene)->key.set(1);
    const bool keptSnapshot = !root(scene)->seat()->needsBranchDeclaration() && r.declared == 1 && r.constructed == 2;
    LOKA_VERIFY(keptSnapshot);
    root(scene)->key.set(2);
    {
      const bool verified = (r.declared == 2 && r.current != old);
      LOKA_VERIFY(verified);
    }
  }
  loka::core::LokaAllocSetBackend(0, 0);
}
void testLazyScopeUnmountCancelsWatch()
{
  Record r;
  record = &r;
  NullScenePlatformController platform;
  loka::core::MutableState<int> source(0);
  loka::core::PushStateTracker tracker;
  tracker.addState(&source);
  r.externalSource = &source;
  {
    Scene scene((Boundary<Root>()));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    const int calls = r.copyCalls;
    {
      loka::core::StateTrackerGuard guard(&tracker);
      tracker.defer(&writePendingSource, &source);
      loka::dsl::testing::SceneTestAccess::unmount(scene);
    }
    LOKA_VERIFY(r.copyCalls == calls);
  }
  {
    const bool verified = (r.destroyed == r.constructed);
    LOKA_VERIFY(verified);
  }
  {
    const bool verified = (platform.ledger().empty());
    LOKA_VERIFY(verified);
  }
}

void testLazyScopeOuterDestroyRecreatesOwner()
{
  Record r;
  record = &r;
  r.outer = true;
  NullScenePlatformController platform;
  Scene scene((Boundary<Root>()));
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  root(scene)->shown.set(false);
  scene.flushInvalidation();
  platform.drainNativeRetirements();
  LOKA_VERIFY(r.destroyed == 1);
  const std::string hiddenDump = loka::dsl::testing::OwnershipDump::dump(scene);
  const bool ownerGone = hiddenDump.find("lazy-scope") == std::string::npos;
  LOKA_VERIFY(ownerGone);
  root(scene)->shown.set(true);
  scene.flushInvalidation();
  platform.drainNativeRetirements();
  const bool recreated = r.constructed == 2 && r.declared == 2 && platform.ledger().size() == 4;
  LOKA_VERIFY(recreated);
}
