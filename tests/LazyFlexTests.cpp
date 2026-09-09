#include "LazyFlexTests.hpp"
#include "support/TestVerify.hpp"
#include "app/nodes/nestable/LazyFlex.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/context/NullTextContext.hpp"
#include "testing/scene/OwnershipDump.hpp"
#include <cstdio>
#include <cstring>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  class CardNode;
  struct CardProps : NodePropsBase<CardProps>
  {
    typedef CardProps TypeTag;
    typedef CardNode NodeType;
    CardProps(const String &text = String(), short n = 0)
        : label(text),
          number(n)
    {
    }
    bool operator<(const PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
        return false;
      const CardProps &other = static_cast<const CardProps &>(rhs);
      return this->number != other.number ? this->number < other.number : this->label.compare(other.label) < 0;
    }
    bool operator!=(const CardProps &other) const
    {
      return !this->label.equals(other.label) || this->number != other.number;
    }
    String label;
    short number;
  };
  struct Record
  {
    Record()
        : list(0),
          second(0),
          viewport(0),
          otherViewport(0),
          editing(false),
          updates(0)
    {
      for (int i = 0; i < 64; ++i)
      {
        constructions[i] = 0;
        bindings[i] = 0;
        cards[i] = 0;
      }
    }
    ObservableList<CardProps> *list, *second;
    State<Frame> *viewport, *otherViewport;
    bool editing;
    int updates;
    int constructions[64], bindings[64];
    CardNode *cards[64];
  };
  Record *record;
  class CardNode : public ComponentNodeWithProps<CardProps>
  {
  public:
    explicit CardNode(const CardProps &p)
        : ComponentNodeWithProps<CardProps>(p),
          label(),
          toggle()
    {
      ++record->constructions[p.number];
      record->cards[p.number] = this;
      this->declareStates(2).state(this->label, p.label).state(this->toggle, false);
    }
    virtual ~CardNode()
    {
      if (record->cards[this->props.number] == this)
        record->cards[this->props.number] = 0;
    }
    virtual void declareBindings(BindingToken &)
    {
      ++record->bindings[this->props.number];
      if (!this->label.get().equals(this->props.label))
        this->label.set(this->props.label);
    }
    virtual void composeChildren(NodeComposition &c)
    {
      if (record->editing)
        c.declare(Row() << Text(this->label.state()) << EditText(this->label));
      else
        c.declare(Row() << Text(this->label.state()) << Button("marker"));
    }
    NodeState<String> label;
    NodeState<bool> toggle;
  };
  class CountedFlex : public LazyFlexNode<CardProps>
  {
  public:
    explicit CountedFlex(const LazyFlexProps<CardProps> &p)
        : LazyFlexNode<CardProps>(p)
    {
    }
    virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
    {
      if (event == COMPOSE_EVENT_UPDATE)
        ++record->updates;
      LazyFlexNode<CardProps>::composeWithContext(context, event);
    }
  };
  class Root : public BoundaryNodeFor<Root>
  {
  public:
    explicit Root(const BoundaryPropsFor<Root> &p)
        : BoundaryNodeFor<Root>(p)
    {
    }
    virtual void composeNode(NodeComposition &c)
    {
      LazyFlexProps<CardProps> p(*record->list);
      p.cells = CanvasProps(200, 20, record->viewport);
      if (record->second)
        c.declare(Column() << BoundaryDefinition<LazyFlexProps<CardProps>, CountedFlex>(p)
                           << LazyColumn(*record->second).cells(200, 20).viewport(*record->otherViewport));
      else
        c.declare(BoundaryDefinition<LazyFlexProps<CardProps>, CountedFlex>(p));
    }
  };
  struct Fixture
  {
    Fixture(unsigned short capacity = 32, int count = 20, bool empty = false, bool edit = false, bool two = false)
        : r(),
          tracker(),
          list(),
          second(),
          view(empty ? Frame() : Frame(0, 0, 200, 160)),
          otherView(Frame(0, 0, 200, 40)),
          platform(),
          scene((Boundary<Root>()))
    {
      record = &this->r;
      this->r.list = &this->list;
      this->r.viewport = &this->view;
      this->r.editing = edit;
      this->tracker.addState(&this->view);
      this->tracker.addState(&this->otherView);
      LOKA_VERIFY(this->list.attach(&this->tracker, capacity) == ATTACH_OK);
      for (int i = 0; i < count; ++i)
        LOKA_VERIFY(this->list.insert(i, CardProps(String("old"), i)) == EDIT_OK);
      if (two)
      {
        LOKA_VERIFY(this->second.attach(&this->tracker, capacity) == ATTACH_OK);
        for (int i = 0; i < count; ++i)
          LOKA_VERIFY(this->second.insert(i, CardProps(String("other"), i + 32)) == EDIT_OK);
        this->r.second = &this->second;
        this->r.otherViewport = &this->otherView;
      }
      this->scene.mount(&this->platform);
      LayoutState bounds;
      bounds.width = 200;
      bounds.height = 400;
      this->platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootBoundary(this->scene), bounds);
      this->scene.updateAttached(true);
      this->drain();
    }
    ~Fixture()
    {
      this->scene.unmount();
      this->drain();
    }
    void drain()
    {
      this->scene.flushInvalidation();
      this->scene.flushInvalidation(); // The next clock boundary reclaims logical retirees silently.
      this->platform.drainNativeRetirements();
    }
    void page(int y)
    {
      {
        StateTrackerGuard guard(&this->tracker);
        this->view.set(Frame(0, y, 200, 160));
      }
      this->drain();
    }
    std::string dump() const
    {
      return loka::dsl::testing::OwnershipDump::dump(this->scene);
    }
    LazyFlexNode<CardProps> *flex()
    {
      return static_cast<LazyFlexNode<CardProps> *>(
          loka::dsl::testing::SceneTestAccess::rootBoundary(this->scene)->childrenHead());
    }
    Record r;
    PushStateTracker tracker;
    ObservableList<CardProps> list, second;
    MutableState<Frame> view, otherView;
    NullScenePlatformController platform;
    Scene scene;
  };
  void rows(Fixture &f, unsigned count)
  {
    {
      const bool ledgerFact = f.platform.ledger().size() == count;
      LOKA_VERIFY(ledgerFact);
    }
    for (size_t i = 0; i < f.platform.ledger().size(); ++i)
    {
      const bool ledgerFact = f.platform.ledger()[i].visible;
      LOKA_VERIFY(ledgerFact);
    }
  }
  TextNode *text(CardNode *card)
  {
    LOKA_VERIFY(card != 0);
    return card->childrenHead()->asNestable()->childrenHead()->asTextNode();
  }
} // namespace

void testLazyFlexViewportCrossesTrackersInOneUpdate()
{
  Fixture f;
  rows(f, 8);
  for (int i = 0; i < 20; ++i)
    LOKA_VERIFY(f.r.constructions[i] == (i < 8 ? 1 : 0));
  const int before = f.r.updates;
  f.page(200);
  rows(f, 8);
  LOKA_VERIFY(f.r.updates == before + 1);
  for (int i = 0; i < 20; ++i)
  {
    LOKA_VERIFY(f.r.constructions[i] == (i < 8 || (i >= 10 && i < 18) ? 1 : 0));
    if (i < 8)
      LOKA_VERIFY(f.r.cards[i] == 0);
  }
}
void testLazyFlexEmptyViewportWaitsForFirstWrite()
{
  Fixture f(32, 20, true);
  rows(f, 0);
  for (int i = 0; i < 20; ++i)
    LOKA_VERIFY(f.r.constructions[i] == 0);
  f.page(0);
  rows(f, 8);
}
void testLazyFlexTenPageFlipsKeepOwnershipBounded()
{
  Fixture f;
  // Show retains its empty false arm once visited; warm both pages first.
  f.page(200);
  f.page(0);
  const std::string before = f.dump();
  for (int i = 0; i < 10; ++i)
  {
    f.page(200);
    rows(f, 8);
    f.page(0);
    rows(f, 8);
    LOKA_VERIFY(f.dump() == before);
  }
  printf("LazyFlex after ten round trips:\n%s", before.c_str());
  printf("LazyFlex host sizes: generation=%lu Derived=%lu evaluator=%lu Show=%lu plan=%lu runtime=%lu LazyItem=%lu "
         "Fragment=%lu\n",
         static_cast<unsigned long>(sizeof(LazyGenerationNode<CardProps>)),
         static_cast<unsigned long>(sizeof(DerivedState<bool>)),
         static_cast<unsigned long>(sizeof(lazy_flex_detail::Visible)),
         static_cast<unsigned long>(sizeof(ShowDefinition)),
         static_cast<unsigned long>(sizeof(BoundaryBranchSeatPlanEntry)),
         static_cast<unsigned long>(sizeof(BoundaryBranchSeatRuntimeEntry)),
         static_cast<unsigned long>(sizeof(LazyItem<CardProps>)),
         static_cast<unsigned long>(sizeof(FragmentNode)));
}
void testLazyFlexVisibleContentAppliesWithoutReconstruction()
{
  Fixture f;
  CardNode *changed = f.r.cards[3];
  LazyItem<CardProps> item(f.list, 3);
  LOKA_VERIFY(item.itemId() == f.list.at(3).id);
  LOKA_VERIFY(item.propsBase() == &f.list.at(3).value);
  CardNode *untouched = f.r.cards[4];
  untouched->toggle.set(true);
  f.drain();
  const int bindings = f.r.bindings[4];
  const unsigned long revision = f.list.revision().get().content;
  LOKA_VERIFY(f.list.update(f.list.at(3).id, CardProps(String("new"), 3)) == EDIT_OK);
  f.drain();
  LOKA_VERIFY(f.r.cards[3] == changed && f.r.constructions[3] == 1);
  LOKA_VERIFY(text(changed)->props.text_->get().equals(String("new")));
  LOKA_VERIFY(f.r.cards[4] == untouched && untouched->toggle.get());
  LOKA_VERIFY(f.r.bindings[4] == bindings);
  LOKA_VERIFY(f.list.revision().get().content == revision + 1);
}
void testLazyFlexHiddenContentIsReadOnFirstMaterialization()
{
  Fixture f;
  LOKA_VERIFY(f.list.update(f.list.at(12).id, CardProps(String("hidden new"), 12)) == EDIT_OK);
  f.drain();
  LOKA_VERIFY(f.r.constructions[12] == 0);
  rows(f, 8);
  f.page(200);
  LOKA_VERIFY(f.r.constructions[12] == 1);
  LOKA_VERIFY(text(f.r.cards[12])->props.text_->get().equals(String("hidden new")));
}
void testLazyFlexStructureReplacesGeneration()
{
  Fixture f;
  Node *old = f.flex()->childrenHead();
  LOKA_VERIFY(f.list.insert(0, CardProps(String("inserted"), 20)) == EDIT_OK);
  f.drain();
  rows(f, 8);
  LOKA_VERIFY(f.flex()->childrenHead() != old);
  for (int i = 0; i < 7; ++i)
    LOKA_VERIFY(f.r.constructions[i] == 2);
  LOKA_VERIFY(f.r.constructions[20] == 1);
  LOKA_VERIFY(f.list.remove(f.list.at(0).id) == EDIT_OK);
  f.drain();
  rows(f, 8);
  LOKA_VERIFY(f.r.cards[20] == 0);
  LOKA_VERIFY(f.list.move(f.list.at(0).id, 19) == EDIT_OK);
  f.drain();
  rows(f, 8);
  LOKA_VERIFY(f.r.cards[0] == 0 && f.r.cards[8] != 0);
  const std::string dump = f.dump();
  const size_t owner = dump.find("lazy-scope");
  LOKA_VERIFY(owner != std::string::npos && dump.find("lazy-scope", owner + 1) == std::string::npos);
}
void testLazyFlexCapacityRefusal()
{
  Fixture f(300);
  LOKA_VERIFY(f.flex()->status() == LAZY_FLEX_CAPACITY_REFUSED);
  rows(f, 0);
  LOKA_VERIFY(f.flex()->childrenHead() == 0);
}
void testLazyFlexEditItemsRetireNativeIdentity()
{
  Fixture f(32, 20, false, true);
  rows(f, 8);
  NullScenePlatformController::FakeControlHandle *old = f.platform.ledger()[3].handle;
  f.page(200);
  rows(f, 8);
  LOKA_VERIFY(old->owner == 0 && old->hitOwner == 0);
  f.page(0);
  rows(f, 8);
  LOKA_VERIFY(f.r.constructions[3] == 2);
}
void testLazyFlexTwoListsStayIndependent()
{
  Fixture f(32, 20, false, false, true);
  rows(f, 10);
  CardNode *other = f.r.cards[32];
  f.page(200);
  rows(f, 10);
  LOKA_VERIFY(f.r.cards[32] == other && f.r.constructions[32] == 1);
  {
    StateTrackerGuard guard(&f.tracker);
    f.otherView.set(Frame(0, 200, 200, 40));
  }
  f.drain();
  rows(f, 10);
  LOKA_VERIFY(f.r.cards[32] == 0 && f.r.cards[42] != 0 && f.r.constructions[10] == 1);
}

void testLazyFlexBatchRefreshesAllVisibleItems()
{
  Fixture f;
  ListOp<CardProps> ops[2] = {ListOp<CardProps>(UPDATE, f.list.at(2).id, 0, CardProps(String("two"), 2)),
                              ListOp<CardProps>(UPDATE, f.list.at(6).id, 0, CardProps(String("six"), 6))};
  ArrayListOpCursor<CardProps> cursor(ops, 2);
  LOKA_VERIFY(f.list.apply(cursor) == EDIT_OK);
  f.drain();
  LOKA_VERIFY(text(f.r.cards[2])->props.text_->get().equals(String("two")));
  LOKA_VERIFY(text(f.r.cards[6])->props.text_->get().equals(String("six")));
  LOKA_VERIFY(f.r.constructions[2] == 1 && f.r.constructions[6] == 1);
  CardNode *outgoing = f.r.cards[5];
  ops[0] = ListOp<CardProps>(REMOVE, f.list.at(0).id);
  ops[1] = ListOp<CardProps>(UPDATE, f.list.at(6).id, 0, CardProps(String("moved six"), 6));
  LOKA_VERIFY(f.list.apply(cursor) == EDIT_OK);
  LOKA_VERIFY(outgoing->props.number == 5);
  f.drain();
  rows(f, 8);
  LOKA_VERIFY(text(f.r.cards[6])->props.text_->get().equals(String("moved six")));
  LOKA_VERIFY(f.r.constructions[6] == 2);
}

namespace
{
  bool refuseStates = false;
  unsigned refusals = 0;
  void *allocateLazyFlex(std::size_t bytes, const LokaAllocationSite &site)
  {
    if (refuseStates && std::strcmp(site.ownerTag, "StateOwner") == 0)
    {
      ++refusals;
      return 0;
    }
    return new (std::nothrow) char[bytes];
  }
  void freeLazyFlex(void *p, const LokaAllocationSite &)
  {
    delete[] static_cast<char *>(p);
  }
} // namespace
void testLazyFlexRefusedGenerationKeepsOldPresentation()
{
  LokaAllocSetBackend(&allocateLazyFlex, &freeLazyFlex);
  {
    Fixture f;
    Node *old = f.flex()->childrenHead();
    refuseStates = true;
    refusals = 0;
    LOKA_VERIFY(f.list.reset() == EDIT_OK);
    // Empty generations need no Derived allocations; instead refill a smaller generation.
    LOKA_VERIFY(f.list.insert(0, CardProps(String("replacement"), 21)) == EDIT_OK);
    f.drain();
    LOKA_VERIFY(refusals > 0 && f.flex()->childrenHead() == old);
    rows(f, 8);
    f.page(200);
    LOKA_VERIFY(f.flex()->childrenHead() == old);
    rows(f, 8);
    refuseStates = false;
    f.page(0);
    f.drain();
    LOKA_VERIFY(f.flex()->childrenHead() != old);
    rows(f, 1);
    LOKA_VERIFY(text(f.r.cards[21])->props.text_->get().equals(String("replacement")));
  }
  LokaAllocSetBackend(0, 0);
}
void testLazyFlexUnmountCancelsForeignWatches()
{
  Fixture f;
  f.scene.unmount();
  f.drain();
  const int before = f.r.updates;
  f.page(200);
  LOKA_VERIFY(f.list.update(f.list.at(12).id, CardProps(String("after unmount"), 12)) == EDIT_OK);
  f.drain();
  LOKA_VERIFY(f.r.updates == before);
  rows(f, 0);
}

void testLazyFlexRowWrapUsesHalfOpenCells()
{
  Fixture f(32, 20, true);
  f.scene.unmount();
  f.drain();
  {
    StateTrackerGuard guard(&f.tracker);
    f.view.set(Frame(0, 0, 80, 80));
  }
  Scene scene(LazyRow(f.list).cells(40, 40).wrap(2).viewport(f.view));
  scene.mount(&f.platform);
  scene.updateAttached(true);
  rows(f, 4);
  for (int i = 0; i < 20; ++i)
    LOKA_VERIFY(f.r.constructions[i] == (i < 4 ? 1 : 0));
  {
    StateTrackerGuard guard(&f.tracker);
    f.view.set(Frame(120, 0, 80, 80));
  }
  scene.flushInvalidation();
  scene.flushInvalidation();
  f.platform.drainNativeRetirements();
  rows(f, 4);
  for (int i = 0; i < 20; ++i)
    LOKA_VERIFY(f.r.constructions[i] == (i < 4 || (i >= 6 && i < 10) ? 1 : 0));
  {
    StateTrackerGuard guard(&f.tracker);
    f.view.set(Frame(INT_MIN, 0, INT_MAX, 80));
  }
  scene.flushInvalidation();
  scene.flushInvalidation();
  f.platform.drainNativeRetirements();
  rows(f, 0);
}
