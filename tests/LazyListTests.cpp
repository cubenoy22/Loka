#include "LazyListTests.hpp"
#include "../example/LazyList/src/MainNode.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include <cstring>

namespace
{
  unsigned constructions[32768];
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;

  Node *find(Node *node, const char *id, unsigned &skip)
  {
    if (!node)
      return 0;
    if (node->testId() == id)
    {
      if (!skip)
        return node;
      --skip;
    }
    INestable *nest = node->asNestable();
    for (Node *child = nest ? nest->childrenHead() : 0; child; child = child->nextInComposition)
    {
      Node *match = find(child, id, skip);
      if (match)
        return match;
    }
    return 0;
  }
  lazylist::CardNode *card(Node *node, short number)
  {
    if (!node)
      return 0;
    if (node->propsTypeId() == lazylist::CardProps::staticTypeId())
    {
      lazylist::CardNode *value = static_cast<lazylist::CardNode *>(node);
      if (value->props.number == number)
        return value;
    }
    INestable *nest = node->asNestable();
    for (Node *child = nest ? nest->childrenHead() : 0; child; child = child->nextInComposition)
    {
      lazylist::CardNode *found = card(child, number);
      if (found)
        return found;
    }
    return 0;
  }
  void reapply(lazylist::CardNode *resident)
  {
    NodeDefinition<lazylist::CardProps, lazylist::CardNode> definition(resident->props);
    LOKA_VERIFY(definition.applyPropsToNode(resident));
  }
  void countWrite(void *context)
  {
    ++*static_cast<unsigned *>(context);
  }
  unsigned markers(Node *node)
  {
    if (!node)
      return 0;
    unsigned count = node->testId() == "LazyList.Marker" ? 1 : 0;
    INestable *nest = node->asNestable();
    for (Node *child = nest ? nest->childrenHead() : 0; child; child = child->nextInComposition)
      count += markers(child);
    return count;
  }
  struct Fixture
  {
    Fixture()
        : model(),
          platform(),
          scene(Boundary<lazylist::MainNode>(lazylist::MainProps(&this->model)))
    {
      std::memset(constructions, 0, sizeof(constructions));
      this->scene.mount(&this->platform);
      LayoutState bounds;
      bounds.width = lazylist::kWindowWidth;
      bounds.height = lazylist::kWindowHeight;
      this->platform.projectLayoutForTesting(this->root(), bounds);
      this->scene.updateAttached(true);
      this->drain();
    }
    ~Fixture()
    {
      this->scene.unmount();
      this->drain();
    }
    Node *root()
    {
      return loka::dsl::testing::SceneTestAccess::rootNode(this->scene);
    }
    Node *node(const char *id, unsigned skip = 0)
    {
      return find(this->root(), id, skip);
    }
    void drain()
    {
      this->scene.flushInvalidation();
      this->scene.flushInvalidation();
      this->platform.drainNativeRetirements();
      {
        const bool fact = !this->scene.hasPendingInvalidation();
        LOKA_VERIFY(fact);
      }
    }
    void click(const char *id, unsigned skip = 0)
    {
      Node *n = this->node(id, skip);
      LOKA_VERIFY(n && n->asButtonNode() && n->asButtonNode()->props.onClick_);
      n->asButtonNode()->props.onClick_->emit();
      this->drain();
    }
    String label(unsigned row)
    {
      Node *n = this->node("LazyList.Label", row);
      LOKA_VERIFY(n && n->asTextNode());
      return n->asTextNode()->props.text_->get();
    }
    void pageLabels(int first)
    {
      LOKA_VERIFY(markers(this->root()) == 8);
      const bool ledgerFact = this->platform.ledger().size() == 14;
      LOKA_VERIFY(ledgerFact);
      for (std::size_t i = 0; i < this->platform.ledger().size(); ++i)
      {
        const bool visible = this->platform.ledger()[i].visible;
        LOKA_VERIFY(visible);
      }
      for (unsigned i = 0; i < 8; ++i)
        LOKA_VERIFY(this->label(i).equals(String::Literal("Card ") + String::FromInt(first + i)));
    }
    lazylist::LazyListModel model;
    NullScenePlatformController platform;
    Scene scene;
  };
} // namespace

namespace lazylist
{
  namespace testing
  {
    void cardConstructed(short number)
    {
      LOKA_VERIFY(number >= 0);
      ++constructions[number];
    }
  } // namespace testing
} // namespace lazylist

void testLazyListPagesAndClamps()
{
  Fixture f;
  f.pageLabels(0);
  for (unsigned i = 0; i < 100; ++i)
    LOKA_VERIFY(constructions[i] == (i < 8 ? 1u : 0u));
  f.click("LazyList.Next");
  f.pageLabels(8);
  LOKA_VERIFY(f.model.viewport().get().y == 160);
  f.click("LazyList.Prev");
  f.pageLabels(0);
  LOKA_VERIFY(constructions[0] == 2);
  f.click("LazyList.Prev");
  LOKA_VERIFY(constructions[0] == 2);
  for (unsigned i = 0; i < 20; ++i)
    f.click("LazyList.Next");
  f.pageLabels(92);
  LOKA_VERIFY(f.model.viewport().get().y == 1840);
  LOKA_VERIFY(f.model.removeFirst() == EDIT_OK);
  f.drain();
  LOKA_VERIFY(f.model.viewport().get().y == 1820);
  f.pageLabels(92);
}

void testLazyListRenamesRetainVisibleStateAndReadHiddenValues()
{
  Fixture f;
  Node *marker = f.node("LazyList.Marker", 3);
  f.click("LazyList.Rename");
  LOKA_VERIFY(f.label(3).equals(String::Literal("Card 3 (1)")));
  LOKA_VERIFY(f.node("LazyList.Marker", 3) == marker);
  LOKA_VERIFY(constructions[3] == 1);
  f.click("LazyList.Marker", 3);
  LOKA_VERIFY(f.label(3).equals(String::Literal("* Card 3 (1)")));
  f.click("LazyList.Rename");
  LOKA_VERIFY(f.label(3).equals(String::Literal("* Card 3 (2)")));
  LOKA_VERIFY(f.node("LazyList.Marker", 3) == marker);
  LOKA_VERIFY(constructions[3] == 1);
  LOKA_VERIFY(f.model.renameCard(12) == EDIT_OK);
  f.drain();
  LOKA_VERIFY(constructions[12] == 0);
  f.click("LazyList.Next");
  LOKA_VERIFY(f.label(4).equals(String::Literal("Card 12 (1)")));
  LOKA_VERIFY(constructions[12] == 1);
  f.click("LazyList.Prev");
  LOKA_VERIFY(f.label(3).equals(String::Literal("Card 3 (2)")));
  LOKA_VERIFY(constructions[3] == 2);
}

void testLazyListStructuralEditsReplaceGeneration()
{
  Fixture f;
  f.click("LazyList.Marker", 1);
  f.click("LazyList.Remove");
  f.pageLabels(1);
  LOKA_VERIFY(constructions[1] == 2);
  LOKA_VERIFY(constructions[8] == 1);
  f.click("LazyList.Marker");
  f.click("LazyList.Insert");
  LOKA_VERIFY(f.label(0).equals(String::Literal("Card 100")));
  LOKA_VERIFY(f.label(1).equals(String::Literal("Card 1")));
  LOKA_VERIFY(constructions[1] == 3);
  LOKA_VERIFY(constructions[100] == 1);
  f.click("LazyList.Marker", 1);
  f.click("LazyList.Move");
  f.pageLabels(1);
  LOKA_VERIFY(constructions[1] == 4);
  LOKA_VERIFY(f.model.cards.at(99).value.number == 100);
}

void testLazyListRefusalsAndUnmount()
{
  Fixture f;
  const ListRevision before = f.model.cards.revision().get();
  f.click("LazyList.Insert");
  LOKA_VERIFY(!(f.model.cards.revision().get() != before));
  LOKA_VERIFY(f.node("LazyList.Result")
                  ->asTextNode()
                  ->props.text_->get()
                  .equals(String::Literal("List full: remove a card before inserting")));
  LOKA_VERIFY(f.model.insertAtTop() == EDIT_CAPACITY_EXCEEDED);
  LOKA_VERIFY(f.model.renameCard(100) == EDIT_INDEX_OUT_OF_RANGE);
  f.scene.unmount();
  f.drain();
  {
    const bool fact = f.platform.ledger().size() == 0;
    LOKA_VERIFY(fact);
  }
  LOKA_VERIFY(f.model.nextPage() == EDIT_OK);
  LOKA_VERIFY(f.model.renameCard(12) == EDIT_OK);
  LOKA_VERIFY(f.model.removeFirst() == EDIT_OK);
  {
    const bool fact = !f.scene.hasPendingInvalidation();
    LOKA_VERIFY(fact);
  }
  while (f.model.cards.size())
    LOKA_VERIFY(f.model.removeFirst() == EDIT_OK);
  LOKA_VERIFY(f.model.removeFirst() == EDIT_ID_NOT_FOUND);
  LOKA_VERIFY(f.model.moveFirstToEnd() == EDIT_ID_NOT_FOUND);
  LOKA_VERIFY(f.model.viewport().get().y == 0);
}

void testLazyListUnchangedBindingsStayQuiet()
{
  Fixture f;
  lazylist::CardNode *resident = card(f.root(), 3);
  LOKA_VERIFY(resident != 0);
  State<String> *label = f.node("LazyList.Label", 3)->asTextNode()->props.text_;
  unsigned writes = 0;
  label->bind(&countWrite, &writes, false);
  reapply(resident);
  f.drain();
  LOKA_VERIFY(writes == 0);
  f.click("LazyList.Rename");
  LOKA_VERIFY(writes == 1);
  reapply(resident);
  f.drain();
  LOKA_VERIFY(writes == 1);
  label->unbind(&countWrite, &writes);
}
