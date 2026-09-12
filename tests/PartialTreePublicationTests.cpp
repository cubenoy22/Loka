#include "PartialTreePublicationTests.hpp"
#include "support/PublishedTreeInvariant.hpp"
#include "support/RecordingPlatformController.hpp"
#include "support/TestVerify.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/scene/node/Conditional.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "testing/scene/OwnershipDump.hpp"
#include "core/LokaAlloc.hpp"
#include <cstring>
#include <new>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using loka::dsl::testing::SceneTestAccess;

  enum Shape
  {
    PLAIN,
    CONDITIONAL,
    NULL_ROOT,
    SECTION
  };

  /** One bounded fixture owns the callback inputs through Scene destruction. */
  struct PublicationFixture
  {
    const Shape shape;
    const int count;
    bool refusing;
    int factoryDepth;
    int attempts;
    int refusals;
    int declarations;
    int bindings;
    loka::core::MutableState<bool> condition;
    std::vector<NodeTag> declared;

    PublicationFixture(Shape s, int n, bool fail)
        : shape(s),
          count(n),
          refusing(fail),
          factoryDepth(0),
          attempts(0),
          refusals(0),
          declarations(0),
          bindings(0),
          condition(s != NULL_ROOT),
          declared()
    {
      if (s == SECTION)
        this->declared.push_back(4101);
      for (int i = 1; i <= n; ++i)
        this->declared.push_back(static_cast<NodeTag>(i));
    }
  };
  PublicationFixture *fixture = 0;

  void *refusingAlloc(std::size_t size, const loka::core::LokaAllocationSite &site)
  {
    if (fixture->refusing && fixture->factoryDepth && std::strcmp(site.ownerTag, "NodeDefinition") == 0)
    {
      ++fixture->refusals;
      return 0;
    }
    return new (std::nothrow) char[size];
  }
  void backendFree(void *p, const loka::core::LokaAllocationSite &)
  {
    delete[] static_cast<char *>(p);
  }

  class FixtureScope
  {
  public:
    explicit FixtureScope(PublicationFixture &value)
    {
      LOKA_VERIFY(!fixture);
      fixture = &value;
      loka::core::LokaAllocSetBackend(&refusingAlloc, &backendFree);
    }
    ~FixtureScope()
    {
      loka::core::LokaAllocSetBackend(0, 0);
      fixture = 0;
    }

  private:
    FixtureScope(const FixtureScope &);
    FixtureScope &operator=(const FixtureScope &);
  };

  struct RefusedChild : FragmentDefinition
  {
    // REFUTE Q1: size zero selects the real heap fallback. Refusal is scoped
    // to the Fragment factory's final NodeDefinition allocation, not reserve.
    virtual size_t nodeSize() const
    {
      return 0;
    }
    virtual Node *create() const
    {
      ++fixture->attempts;
      ++fixture->factoryDepth;
      Node *node = FragmentDefinition::create();
      --fixture->factoryDepth;
      return node;
    }
    virtual NodeDefinitionBase *clone() const
    {
      return new RefusedChild(*this);
    }
  };

  struct SectionStateTag
  {
  };
  class SectionStateNode;
  struct SectionStateProps : NodePropsBase<SectionStateProps>
  {
    typedef SectionStateTag TypeTag;
    typedef SectionStateNode NodeType;
    bool operator<(const PropsBase &) const
    {
      return false;
    }
  };

  class SectionStateNode : public ComponentNodeWithProps<SectionStateProps>
  {
  public:
    explicit SectionStateNode(const SectionStateProps &props)
        : ComponentNodeWithProps<SectionStateProps>(props),
          value_()
    {
      this->state(this->value_, 17);
    }
    const loka::core::State<int> *value() const
    {
      return this->value_.state();
    }
    IStateOwner *owner() const
    {
      return this->value_.dangerouslyOwner();
    }

  protected:
    virtual void composeChildren(NodeComposition &composition)
    {
      composition.declare(FragmentDefinition());
    }

  private:
    NodeState<int> value_;
  };

  class Root : public BoundaryNodeFor<Root>
  {
  public:
    explicit Root(const BoundaryPropsFor<Root> &props)
        : BoundaryNodeFor<Root>(props)
    {
    }
    virtual void declareBindings(BindingToken &)
    {
      ++fixture->bindings;
    }
    virtual void composeNode(NodeComposition &composition)
    {
      ++fixture->declarations;
      FragmentDefinition branch;
      for (int i = 1; i <= fixture->count; ++i)
      {
        if (fixture->shape != SECTION && i >= (fixture->count == 100 ? 61 : 2))
        {
          RefusedChild child;
          child.tag(static_cast<NodeTag>(i));
          branch << child;
        }
        else
          branch << FragmentDefinition().tag(static_cast<NodeTag>(i));
      }
      FragmentDefinition empty;
      switch (fixture->shape)
      {
      case PLAIN:
        composition.declare(branch);
        break;
      case CONDITIONAL:
        composition.declare(ConditionalDefinition(ConditionalProps(&fixture->condition, &branch, &empty)));
        break;
      case NULL_ROOT:
      {
        RefusedChild target;
        target.tag(1);
        composition.declare(ConditionalDefinition(ConditionalProps(&fixture->condition, &target, &empty)));
        break;
      }
      case SECTION:
      {
        Section section(4101);
        section << branch << NodeDefinition<SectionStateProps, SectionStateNode>();
        composition.declare(ConditionalDefinition(ConditionalProps(&fixture->condition, &section, &empty)));
        break;
      }
      }
    }
  };

  /** Copy the publication facts synchronously; never revisit a retired root. */
  class PublicationObserver : public SceneTestSupport::RecordingPlatformController
  {
  public:
    std::vector<NodeTag> published;
    virtual void onChange(Node *root, NodeDirtyFlags flags, bool rebuild)
    {
      this->published.clear();
      SceneTestSupport::CollectPublishedTags(root, this->published);
      SceneTestSupport::RecordingPlatformController::onChange(root, flags, rebuild);
    }
  };

  void refresh(Scene &scene)
  {
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    scene.flushInvalidation();
  }

  bool check(const char *stage, Scene &scene, PublicationObserver &platform)
  {
    std::vector<NodeTag> logical;
    SceneTestSupport::CollectPublishedTags(SceneTestAccess::rootBoundary(scene), logical);
    const bool white = SceneTestAccess::whiteFlagFullRebuildPending(scene);
    std::fprintf(stderr,
                 "%s logical=%lu published=%lu/%lu white=%d publications=%lu attempts=%d refusals=%d declarations=%d "
                 "bindings=%d\n",
                 stage,
                 static_cast<unsigned long>(logical.size()),
                 static_cast<unsigned long>(platform.published.size()),
                 static_cast<unsigned long>(fixture->declared.size()),
                 white,
                 static_cast<unsigned long>(platform.changeCount()),
                 fixture->attempts,
                 fixture->refusals,
                 fixture->declarations,
                 fixture->bindings);
    if (fixture->shape == CONDITIONAL)
      std::fprintf(stderr,
                   "%s",
                   loka::dsl::testing::OwnershipDump::dumpSeatRuntime(*SceneTestAccess::rootBoundary(scene)).c_str());
    return SceneTestSupport::PublishedTreeMatchesDeclarationOrWhiteFlag(
        platform.published, fixture->declared, white, stage);
  }

  void partialPin(Shape shape, int count)
  {
    PublicationFixture data(shape, count, true);
    FixtureScope scope(data);
    PublicationObserver platform;
    Scene scene((Boundary<Root>()));
    scene.mount(&platform);
    SceneTestAccess::updateAttached(scene, true);
    // Instrumented Q1/Q3 trace: ATTACH arms white and withholds publication.
    const bool initialWhitePending = SceneTestAccess::whiteFlagFullRebuildPending(scene);
    LOKA_VERIFY(initialWhitePending);
    const bool initialPublicationWithheld = platform.changeCount() == 0;
    LOKA_VERIFY(initialPublicationWithheld);
    LOKA_VERIFY(data.refusals > 0 && data.attempts == data.refusals);
    const bool initialPublicationEmpty = platform.published.empty();
    LOKA_VERIFY(initialPublicationEmpty);
    const int initialAttempts = data.attempts;
    bool valid = check("ATTACH", scene, platform);
    const int bindings = data.bindings;
    const char *stages[] = {"refusing-refresh-1", "refusing-refresh-2", "refusing-refresh-3"};
    for (int i = 0; i < (count == 100 ? 3 : 1); ++i)
    {
      refresh(scene);
      valid = check(stages[i], scene, platform) && valid;
    }
    data.refusing = false;
    refresh(scene);
    valid = check("healed-refresh", scene, platform) && valid;
    // Keep collecting all three persistent-refusal observations and healing
    // before failing, so a red run shows both publication and missing retry.
    LOKA_VERIFY(data.declarations == 1 && data.bindings == bindings);
    LOKA_VERIFY(valid);
    LOKA_VERIFY(data.attempts > initialAttempts);
    LOKA_VERIFY(platform.published == data.declared);
    const bool healedWhiteCleared = !SceneTestAccess::whiteFlagFullRebuildPending(scene);
    LOKA_VERIFY(healedWhiteCleared);
  }
} // namespace

void testPartialTree629()
{
  partialPin(PLAIN, 2);
}
void testExpectedRedPartialTree144()
{
  partialPin(CONDITIONAL, 2);
}
void testPartialTree100Children()
{
  partialPin(PLAIN, 100);
}

void testPartialTreeHealthyControl()
{
  PublicationFixture data(PLAIN, 2, false);
  FixtureScope scope(data);
  PublicationObserver platform;
  Scene scene((Boundary<Root>()));
  scene.mount(&platform);
  SceneTestAccess::updateAttached(scene, true);
  LOKA_VERIFY(check("healthy-ATTACH", scene, platform));
  const bool healthyInitialCounts = platform.changeCount() == 1 && data.attempts == 1 && data.refusals == 0;
  LOKA_VERIFY(healthyInitialCounts);
  Node *first = SceneTestAccess::rootBoundary(scene)->childrenHead();
  Node *childOne = first->asNestable()->childrenHead();
  Node *childTwo = childOne->nextInComposition;
  const int bindings = data.bindings;
  refresh(scene);
  LOKA_VERIFY(check("healthy-refresh", scene, platform));
  const bool rootChildRetained = first == SceneTestAccess::rootBoundary(scene)->childrenHead();
  LOKA_VERIFY(rootChildRetained);
  const bool childrenRetained = first->asNestable()->childrenHead() == childOne && childOne->nextInComposition == childTwo;
  LOKA_VERIFY(childrenRetained);
  LOKA_VERIFY(data.declarations == 1 && data.bindings == bindings && data.attempts == 1);
  const bool healthyRefreshPublished = platform.changeCount() == 2;
  LOKA_VERIFY(healthyRefreshPublished);
}

void testPartialTreeNullRootSwitchRecovery()
{
  PublicationFixture data(NULL_ROOT, 1, true);
  FixtureScope scope(data);
  PublicationObserver platform;
  Scene scene((Boundary<Root>()));
  scene.mount(&platform);
  SceneTestAccess::updateAttached(scene, true);
  const bool initialSwitchCounts = platform.changeCount() == 1 && data.attempts == 0;
  LOKA_VERIFY(initialSwitchCounts);
  const int bindings = data.bindings;
  {
    loka::core::StateTrackerGuard guard(SceneTestAccess::rootBoundary(scene)->tracker());
    data.condition.set(true);
  }
  LOKA_VERIFY(check("switch-refused", scene, platform));
  const bool refusedSwitchCounts = data.attempts == 1 && data.refusals == 1 && platform.changeCount() == 1;
  LOKA_VERIFY(refusedSwitchCounts);
  refresh(scene);
  LOKA_VERIFY(check("switch-refusing-refresh", scene, platform));
  const bool refusingRefreshCounts = data.attempts == 2 && data.refusals == 2 && platform.changeCount() == 1;
  LOKA_VERIFY(refusingRefreshCounts);
  data.refusing = false;
  refresh(scene);
  LOKA_VERIFY(check("switch-healed", scene, platform));
  const bool healedSwitchCounts = data.attempts == 3 && data.refusals == 2 && platform.changeCount() == 2;
  LOKA_VERIFY(healedSwitchCounts);
  LOKA_VERIFY(platform.published == data.declared);
  LOKA_VERIFY(data.declarations == 1 && data.bindings == bindings);
}

void testPartialTreeConditionalSectionRoundTrip271()
{
  PublicationFixture data(SECTION, 2, false);
  FixtureScope scope(data);
  PublicationObserver platform;
  Scene scene((Boundary<Root>()));
  scene.mount(&platform);
  SceneTestAccess::updateAttached(scene, true);
  BoundaryNode *boundary = SceneTestAccess::rootBoundary(scene);
  Node *section = boundary->childrenHead();
  const bool sectionPresent = section && section->asBoundarySectionNode();
  LOKA_VERIFY(sectionPresent);
  // State belongs to the Section owner; the stable pointer is borrowed only
  // through this retained park/re-entry round trip, never across reclamation.
  BoundarySectionNode *owner = section->asBoundarySectionNode();
  SectionStateNode *component =
      static_cast<SectionStateNode *>(section->asNestable()->childrenHead()->nextInComposition);
  const loka::core::State<int> *state = component->value();
  const bool stateOwnedBySection = state && component->owner() == owner;
  LOKA_VERIFY(stateOwnedBySection);
  LOKA_VERIFY(check("section-ATTACH", scene, platform));
  const int bindings = data.bindings;
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    data.condition.set(false);
  }
  scene.flushInvalidation();
  const bool sectionParked = boundary->childrenHead() != section;
  LOKA_VERIFY(sectionParked);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    data.condition.set(true);
  }
  const bool sectionReturned = boundary->childrenHead() == section;
  LOKA_VERIFY(sectionReturned);
  const bool sectionOwnerRetained = boundary->childrenHead()->asBoundarySectionNode() == owner;
  LOKA_VERIFY(sectionOwnerRetained);
  const bool sectionStateRetained = component->owner() == owner && component->value() == state && state->get() == 17;
  LOKA_VERIFY(sectionStateRetained);
  LOKA_VERIFY(check("section-reentry", scene, platform));
  refresh(scene);
  const bool refreshedSectionStateRetained = boundary->childrenHead() == section && state->get() == 17;
  LOKA_VERIFY(refreshedSectionStateRetained);
  LOKA_VERIFY(check("section-refresh", scene, platform));
  LOKA_VERIFY(data.declarations == 1 && data.bindings == bindings);
}

namespace
{
  enum FreshCaptureDoor
  {
    FRESH_FACTORY,
    FRESH_ROOT_CLONE,
    FRESH_CHILD_CLONE,
    FRESH_COPY_CLONE,
    FRESH_EMPTY
  };

  struct FreshFixture
  {
    const FreshCaptureDoor door;
    const NodeTag refusedTag;
    bool refusing;
    NodeTag factoryTag;
    int declarations;
    int bindings;
    int cloneRefusals;
    int factoryRefusals;
    int factoryAttempts;
    int gateLive;
    int leafAlive;
    int attaches;
    int detaches;
    std::vector<NodeTag> declared;

    FreshFixture(FreshCaptureDoor captureDoor, NodeTag tag)
        : door(captureDoor), refusedTag(tag), refusing(true), factoryTag(0),
          declarations(0), bindings(0), cloneRefusals(0), factoryRefusals(0),
          factoryAttempts(0), gateLive(0), leafAlive(0), attaches(0), detaches(0), declared()
    {
      if (door != FRESH_EMPTY)
        for (NodeTag i = 1; i <= 3; ++i)
          this->declared.push_back(i);
    }
  };
  FreshFixture *fresh = 0;

  void *freshAlloc(std::size_t size, const loka::core::LokaAllocationSite &site)
  {
    if (std::strcmp(site.ownerTag, "NodeDefinition") == 0)
    {
      if (fresh->door == FRESH_FACTORY && fresh->refusing && fresh->factoryTag == fresh->refusedTag)
      {
        ++fresh->factoryRefusals;
        return 0;
      }
      ++fresh->gateLive;
    }
    return new (std::nothrow) char[size];
  }
  void freshFree(void *storage, const loka::core::LokaAllocationSite &site)
  {
    if (std::strcmp(site.ownerTag, "NodeDefinition") == 0)
      --fresh->gateLive;
    delete[] static_cast<char *>(storage);
  }

  class FreshFixtureScope
  {
  public:
    explicit FreshFixtureScope(FreshFixture &data)
    {
      assert(!fresh);
      fresh = &data;
      loka::core::LokaAllocSetBackend(&freshAlloc, &freshFree);
    }
    ~FreshFixtureScope()
    {
      loka::core::LokaAllocSetBackend(0, 0);
      fresh = 0;
    }
  private:
    FreshFixtureScope(const FreshFixtureScope &);
    FreshFixtureScope &operator=(const FreshFixtureScope &);
  };

  class FreshLeaf;
  struct FreshLeafTag {};
  struct FreshLeafProps : NodePropsBase<FreshLeafProps>
  {
    typedef FreshLeafTag TypeTag;
    typedef FreshLeaf NodeType;
    bool operator<(const PropsBase &) const { return false; }
  };
  class FreshLeaf : public ComponentNodeWithProps<FreshLeafProps>
  {
  public:
    explicit FreshLeaf(const FreshLeafProps &props) : ComponentNodeWithProps<FreshLeafProps>(props)
    {
      ++fresh->leafAlive;
    }
    virtual ~FreshLeaf() { --fresh->leafAlive; }
  protected:
    virtual void composeChildren(NodeComposition &) {}
    virtual void attachNode(NodeComposition &) { ++fresh->attaches; }
    virtual void detachNode(NodeComposition &composition)
    {
      ++fresh->detaches;
      ComponentNode::detachNode(composition);
    }
  };
  struct FreshLeafDefinition : NodeDefinition<FreshLeafProps, FreshLeaf>
  {
    virtual size_t nodeSize() const { return 0; }
    virtual Node *create() const
    {
      ++fresh->factoryAttempts;
      fresh->factoryTag = this->nodeTag();
      Node *node = NodeDefinition<FreshLeafProps, FreshLeaf>::create();
      fresh->factoryTag = 0;
      return node;
    }
    virtual NodeDefinitionBase *clone() const
    {
      if ((fresh->door == FRESH_CHILD_CLONE || fresh->door == FRESH_COPY_CLONE) && fresh->refusing)
      {
        ++fresh->cloneRefusals;
        return 0;
      }
      return new FreshLeafDefinition(*this);
    }
  };
  struct FreshRootDefinition : FragmentDefinition
  {
    virtual NodeDefinitionBase *clone() const
    {
      if (fresh->door == FRESH_ROOT_CLONE && fresh->refusing)
      {
        ++fresh->cloneRefusals;
        return 0;
      }
      return new FreshRootDefinition(*this);
    }
  };
  void declareFreshChildren(FreshRootDefinition &root)
  {
    for (NodeTag i = 1; i <= 3; ++i)
    {
      FreshLeafDefinition leaf;
      leaf.setNodeTag(i);
      root.addChild(&leaf);
    }
  }
  class FreshBoundary : public BoundaryNodeFor<FreshBoundary>
  {
  public:
    explicit FreshBoundary(const BoundaryPropsFor<FreshBoundary> &props) : BoundaryNodeFor<FreshBoundary>(props) {}
    virtual void declareBindings(BindingToken &) { ++fresh->bindings; }
    virtual void composeNode(NodeComposition &composition)
    {
      ++fresh->declarations;
      if (fresh->door == FRESH_EMPTY)
        return;
      FreshRootDefinition root;
      const bool refusing = fresh->refusing;
      if (fresh->door == FRESH_COPY_CLONE)
        fresh->refusing = false;
      declareFreshChildren(root);
      fresh->refusing = refusing;
      if (fresh->door == FRESH_COPY_CLONE)
        composition.declareTagged(NODE_TAG_NONE, root);
      else
        composition.declare(root);
    }
  };

  bool freshInvariant(Scene &scene, PublicationObserver &platform, const char *stage)
  {
    return SceneTestSupport::PublishedTreeMatchesDeclarationOrWhiteFlag(
        platform.published, fresh->declared,
        SceneTestAccess::whiteFlagFullRebuildPending(scene), stage);
  }

  void freshRefusalPin(FreshCaptureDoor door, NodeTag refusedTag, bool plainRoot = false)
  {
    FreshFixture data(door, refusedTag);
    FreshFixtureScope scope(data);
    {
      PublicationObserver platform;
      NodeDefinitionBase *definition;
      if (plainRoot)
      {
        FreshRootDefinition *root = new FreshRootDefinition();
        declareFreshChildren(*root);
        definition = root;
      }
      else
        definition = Boundary<FreshBoundary>().clone();
      Scene scene(definition);
      scene.mount(&platform);
      SceneTestAccess::updateAttached(scene, true);
      BoundaryNode *boundary = SceneTestAccess::rootBoundary(scene);
      LOKA_VERIFY(freshInvariant(scene, platform, "fresh-refusal"));
      assert(SceneTestAccess::whiteFlagFullRebuildPending(scene)); // loka-assert-ok: pure accessor
      assert(!boundary->childrenHead()); // loka-assert-ok: pure accessor
      assert(platform.changeCount() == 0); // loka-assert-ok: pure accessor
      assert(data.attaches == 0 && data.detaches == 0);
      if (door == FRESH_FACTORY)
      {
        assert(data.factoryRefusals == 1 && data.factoryAttempts == 3);
        assert(data.leafAlive == 2);
      }
      else
      {
        // Actual clone-refusal instrumentation distinguishes this from an
        // intentional empty declaration: no factory was reached at all.
        assert(data.cloneRefusals > 0 && data.factoryAttempts == 0);
      }
      const int ownerBaseline = data.gateLive - data.leafAlive;
      (void)ownerBaseline;
      boundary->drainRetiredSubtreesAtNextTrackerRun();
      assert(data.leafAlive == 0 && data.gateLive == ownerBaseline);
      assert(data.detaches == 0);
      const int firstDeclarations = data.declarations;
      refresh(scene);
      LOKA_VERIFY(freshInvariant(scene, platform, "fresh-persistent-refusal"));
      assert(SceneTestAccess::whiteFlagFullRebuildPending(scene)); // loka-assert-ok: pure accessor
      assert(!boundary->childrenHead()); // loka-assert-ok: pure accessor
      assert(data.attaches == 0 && data.detaches == 0);
      boundary->drainRetiredSubtreesAtNextTrackerRun();
      assert(data.leafAlive == 0 && data.gateLive == ownerBaseline);
      data.refusing = false;
      refresh(scene);
      LOKA_VERIFY(freshInvariant(scene, platform, "fresh-healed"));
      assert(!SceneTestAccess::whiteFlagFullRebuildPending(scene)); // loka-assert-ok: pure accessor
      assert(platform.published == data.declared);
      assert(data.leafAlive == 3 && data.attaches == 3);
      const int expectedDeclarations = plainRoot ? 0 : (door == FRESH_FACTORY ? firstDeclarations : 3);
      std::fprintf(stderr,
                   "fresh recovery door=%d plain=%d declarations=%d bindings=%d expected=%d cloneRefusals=%d factoryRefusals=%d\n",
                   static_cast<int>(door), plainRoot, data.declarations, data.bindings, expectedDeclarations,
                   data.cloneRefusals, data.factoryRefusals);
      assert(data.declarations == expectedDeclarations);
      assert(data.bindings == expectedDeclarations);
      const int attempts = data.factoryAttempts;
      (void)attempts;
      refresh(scene);
      LOKA_VERIFY(freshInvariant(scene, platform, "fresh-settled"));
      assert(data.declarations == expectedDeclarations && data.factoryAttempts == attempts);
    }
    assert(data.leafAlive == 0 && data.gateLive == 0);
  }
}

void testPartialTreeFirstFactoryRefusal() { freshRefusalPin(FRESH_FACTORY, 1); }
void testPartialTreeMiddleFactoryRefusal() { freshRefusalPin(FRESH_FACTORY, 2); }
void testPartialTreeLastFactoryRefusal() { freshRefusalPin(FRESH_FACTORY, 3); }
void testPartialTreePlainRootFactoryRefusal() { freshRefusalPin(FRESH_FACTORY, 2, true); }
void testPartialTreePlainRootCloneRefusal() { freshRefusalPin(FRESH_ROOT_CLONE, 2, true); }
void testPartialTreeInitialRootCloneRefusal() { freshRefusalPin(FRESH_ROOT_CLONE, 2); }
void testPartialTreeInitialTaggedCopyRefusal() { freshRefusalPin(FRESH_COPY_CLONE, 2); }
void testPartialTreeInitialChildCloneRefusal() { freshRefusalPin(FRESH_CHILD_CLONE, 2); }
void testPartialTreeEmptyDeclarationComposesOnce()
{
  FreshFixture data(FRESH_EMPTY, 0);
  FreshFixtureScope scope(data);
  PublicationObserver platform;
  Scene scene((Boundary<FreshBoundary>()));
  scene.mount(&platform);
  SceneTestAccess::updateAttached(scene, true);
  LOKA_VERIFY(freshInvariant(scene, platform, "empty-attach"));
  assert(!SceneTestAccess::whiteFlagFullRebuildPending(scene)); // loka-assert-ok: pure accessor
  refresh(scene);
  LOKA_VERIFY(freshInvariant(scene, platform, "empty-refresh"));
  assert(data.declarations == 1 && data.bindings == 1 && data.factoryAttempts == 0);
}
