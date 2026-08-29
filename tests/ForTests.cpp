#include "ForTests.hpp"
#include "support/TestVerify.hpp"

#include <cstddef>
#include <cstdio>
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/For.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/composition/NodeCompositionCompare.hpp"
#include "app/scene/composition/NodeCompositionSnapshot.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "core/State.hpp"
#include "core/Vector.hpp"
#include "support/RecomposingBoundary.hpp"
#include "support/RecordingPlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  struct ForItem
  {
    ForItem(int idValue = 0, const char *labelValue = "")
        : id(idValue),
          label(labelValue)
    {
    }

    int id;
    const char *label;
  };

  struct ForTextFactory
  {
    loka::app::TextDefinition operator()(const ForItem &item, std::size_t) const
    {
      return loka::app::Text(item.label);
    }
  };

  class ForComponentNode;
  struct ForComponentTypeTag
  {
  };

  struct ForComponentProps
      : public loka::app::scene::NodePropsBase<ForComponentProps>
  {
    typedef ForComponentTypeTag TypeTag;
    typedef ForComponentNode NodeType;

    ForComponentProps(int idValue = 0, const char *labelValue = "")
        : id(idValue),
          label(labelValue)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return this->propsTypeId() < rhs.propsTypeId();
      }
      const ForComponentProps &other =
          static_cast<const ForComponentProps &>(rhs);
      if (this->id != other.id)
      {
        return this->id < other.id;
      }
      return this->label < other.label;
    }

    int id;
    const char *label;
  };

  class ForComponentNode
      : public loka::app::scene::ComponentNodeWithProps<ForComponentProps>
  {
  public:
    typedef ForComponentTypeTag TypeTag;

    explicit ForComponentNode(const ForComponentProps &props)
        : loka::app::scene::ComponentNodeWithProps<ForComponentProps>(props)
    {
    }

  protected:
    virtual void composeChildren(
        loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::Text(this->props.label));
    }
  };

  class FailingCloneNode;
  struct FailingCloneTypeTag
  {
  };

  struct FailingCloneProps
      : public loka::app::scene::NodePropsBase<FailingCloneProps>
  {
    typedef FailingCloneTypeTag TypeTag;
    typedef FailingCloneNode NodeType;

    explicit FailingCloneProps(bool shouldFail = false)
        : fail(shouldFail)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return this->propsTypeId() < rhs.propsTypeId();
      }
      return this->fail < static_cast<const FailingCloneProps &>(rhs).fail;
    }

    bool fail;
  };

  class FailingCloneNode : public loka::app::scene::Node
  {
  public:
    typedef FailingCloneTypeTag TypeTag;
    explicit FailingCloneNode(const FailingCloneProps &props)
        : props(props)
    {
    }

    FailingCloneProps props;
  };

  struct FailingCloneDefinition
      : public loka::app::scene::NodeDefinition<FailingCloneProps,
                                                FailingCloneNode>
  {
    explicit FailingCloneDefinition(bool fail = false)
        : loka::app::scene::NodeDefinition<FailingCloneProps,
                                           FailingCloneNode>(
              FailingCloneProps(fail))
    {
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return this->props.fail ? 0 : new FailingCloneDefinition(false);
    }
  };

  struct FailingCloneFactory
  {
    explicit FailingCloneFactory(int failingIndex = -1)
        : failingIndex_(failingIndex)
    {
    }

    FailingCloneDefinition operator()(const ForItem &, std::size_t index) const
    {
      return FailingCloneDefinition(
          static_cast<int>(index) == this->failingIndex_);
    }

    int failingIndex_;
  };

  struct CountingTextFactory
  {
    explicit CountingTextFactory(int *calls = 0)
        : calls_(calls)
    {
    }

    loka::app::TextDefinition operator()(const ForItem &item,
                                         std::size_t) const
    {
      if (this->calls_)
      {
        ++*this->calls_;
      }
      return loka::app::Text(item.label);
    }

    int *calls_;
  };

  struct RecordingIndexTextFactory
  {
    RecordingIndexTextFactory(std::size_t *indexes = 0, std::size_t *count = 0)
        : indexes_(indexes),
          count_(count)
    {
    }

    loka::app::TextDefinition operator()(const ForItem &item,
                                         std::size_t index) const
    {
      if (this->indexes_ && this->count_)
      {
        this->indexes_[*this->count_] = index;
        ++*this->count_;
      }
      return loka::app::Text(item.label);
    }

    std::size_t *indexes_;
    std::size_t *count_;
  };

  class ForWindowBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ForWindowBoundaryNode>
      ForWindowBoundaryProps;

  class ForWindowBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            ForWindowBoundaryNode,
            ForWindowBoundaryProps>
  {
  public:
    explicit ForWindowBoundaryNode(const ForWindowBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<
              ForWindowBoundaryNode,
              ForWindowBoundaryProps>(props),
          items_(),
          first_(0)
    {
      this->items_.push_back(ForItem(0, "zero"));
      this->items_.push_back(ForItem(1, "one"));
      this->items_.push_back(ForItem(2, "two"));
      this->items_.push_back(ForItem(3, "three"));
      this->items_.push_back(ForItem(4, "four"));
      this->items_.push_back(ForItem(5, "five"));
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Column column;
      column << loka::app::For(1200, this->items_, ForTextFactory())
                    .window(this->first_, 4);
      composition.declare(column);
    }

    void setFirst(long first)
    {
      this->first_ = first;
    }

    loka::app::ColumnNode *column() const
    {
      loka::app::scene::Node *root = this->compositionRootNode();
      return root ? root->asColumnNode() : 0;
    }

    loka::app::BoundarySectionNode *section(
        loka::app::scene::NodeTag tag) const
    {
      loka::app::ColumnNode *columnNode = this->column();
      for (loka::app::scene::Node *child =
               columnNode ? columnNode->childrenHead() : 0;
           child;
           child = child->nextInComposition)
      {
        if (child->nodeTag() == tag)
        {
          return child->asBoundarySectionNode();
        }
      }
      return 0;
    }

  private:
    loka::Vector<ForItem> items_;
    long first_;
  };

  void assertEquivalentDefinitionTree(
      const loka::app::scene::NodeDefinitionBase *expected,
      const loka::app::scene::NodeDefinitionBase *actual)
  {
    LOKA_VERIFY(expected && actual);
    LOKA_VERIFY(expected->nodeTag() == actual->nodeTag());
    LOKA_VERIFY(expected->propsBase() && actual->propsBase());
    LOKA_VERIFY(expected->propsBase()->propsTypeId() ==
                actual->propsBase()->propsTypeId());
    LOKA_VERIFY(expected->hasEquivalentProps(*actual));

    loka::app::scene::INestableDefinition *expectedNestable =
        const_cast<loka::app::scene::NodeDefinitionBase *>(expected)
            ->asNestableDefinition();
    loka::app::scene::INestableDefinition *actualNestable =
        const_cast<loka::app::scene::NodeDefinitionBase *>(actual)
            ->asNestableDefinition();
    LOKA_VERIFY((expectedNestable != 0) == (actualNestable != 0));
    if (!expectedNestable)
    {
      return;
    }

    LOKA_VERIFY(expectedNestable->childrenCount() ==
                actualNestable->childrenCount());
    loka::app::scene::NodeDefinitionBase *expectedChild =
        expectedNestable->childrenHead();
    loka::app::scene::NodeDefinitionBase *actualChild =
        actualNestable->childrenHead();
    while (expectedChild && actualChild)
    {
      assertEquivalentDefinitionTree(expectedChild, actualChild);
      expectedChild = expectedChild->nextInComposition;
      actualChild = actualChild->nextInComposition;
    }
    LOKA_VERIFY(!expectedChild && !actualChild);
  }

  loka::app::scene::NodeCompositionDiff::Entry *findDiffEntry(
      loka::app::scene::NodeCompositionDiff &diff,
      loka::app::scene::NodeTag tag)
  {
    for (loka::app::scene::NodeCompositionDiff::Entry *entry =
             diff.entriesHead();
         entry;
         entry = entry->nextInComposition)
    {
      if (entry->tag == tag)
      {
        return entry;
      }
    }
    return 0;
  }

  void captureForSnapshot(
      const loka::Vector<ForItem> &items,
      bool derivedKeys,
      loka::app::scene::NodeCompositionSnapshot &snapshot)
  {
    loka::app::Fragment root;
    if (derivedKeys)
    {
      typedef loka::app::ForBuilder<
          ForItem,
          ForTextFactory,
          loka::dsl::Expr<int, loka::dsl::IndexExpr> > DefaultBuilder;
      ForTextFactory factory;
      DefaultBuilder builder = loka::app::For(100, items, factory);
      root << builder.key(builder.slot.member<int, &ForItem::id>());
    }
    else
    {
      root << loka::app::For(100, items, ForTextFactory());
    }

    loka::app::scene::NodeComposition composition;
    composition.declare(root);
    snapshot.capture(composition);
  }
} // namespace

void testForIndexBuildsHandWrittenSectionTree()
{
  loka::Vector<ForComponentProps> items;
  items.push_back(ForComponentProps(1, "one"));
  items.push_back(ForComponentProps(2, "two"));
  items.push_back(ForComponentProps(3, "three"));

  loka::app::Fragment generated;
  generated << loka::app::For(400, items);

  loka::app::Fragment expected;
  for (std::size_t i = 0; i < items.size(); ++i)
  {
    loka::app::Section section(
        static_cast<loka::app::scene::NodeTag>(400 + i));
    section << loka::app::scene::Component(items[i]);
    expected << section;
  }

  assertEquivalentDefinitionTree(&expected, &generated);

  loka::core::MutableState<bool> visible(true);
  loka::app::ShowDefinition show = loka::app::Show(visible);
  show << loka::app::For(500, items);
  LOKA_VERIFY(show.childrenCount() == 3);

  loka::app::PolicyScopeDefinition policy;
  policy << loka::app::For(600, items);
  loka::app::scene::INestableDefinition *policyContent =
      policy.scopedBranchDefinition()->asNestableDefinition();
  LOKA_VERIFY(policyContent && policyContent->childrenCount() == 3);
}

void testForRejectsDuplicateKeysWithinBatch()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  LOKA_VERIFY(child >= 0);
  if (child == 0)
  {
    loka::Vector<ForItem> items;
    items.push_back(ForItem(1, "one"));
    items.push_back(ForItem(2, "two"));
    loka::app::Fragment parent;
    typedef loka::app::ForBuilder<
        ForItem,
        ForTextFactory,
        loka::dsl::Expr<int, loka::dsl::IndexExpr> > DefaultBuilder;
    ForTextFactory factory;
    DefaultBuilder builder = loka::app::For(700, items, factory);
    parent << builder.key(loka::dsl::Const(0));
    _exit(0);
  }

  int status = 0;
  LOKA_VERIFY(waitpid(child, &status, 0) == child);
  LOKA_VERIFY(WIFSIGNALED(status));
  LOKA_VERIFY(WTERMSIG(status) == SIGABRT);
#endif
}

void testForRejectsInvalidTagsBeforeInsertion()
{
  loka::Vector<ForItem> items;
  items.push_back(ForItem(1, "one"));

  int factoryCalls = 0;
  CountingTextFactory factory(&factoryCalls);

  loka::app::Fragment zeroBaseParent;
  zeroBaseParent << loka::app::For(0, items, factory);
  LOKA_VERIFY(zeroBaseParent.childrenCount() == 0);
  LOKA_VERIFY(factoryCalls == 0);

  loka::app::Fragment tooLargeBaseParent;
  tooLargeBaseParent << loka::app::For(65537L, items, factory);
  LOKA_VERIFY(tooLargeBaseParent.childrenCount() == 0);
  LOKA_VERIFY(factoryCalls == 0);

  loka::app::Fragment negativeBaseParent;
  negativeBaseParent << loka::app::For(-1L, items, factory);
  LOKA_VERIFY(negativeBaseParent.childrenCount() == 0);
  LOKA_VERIFY(factoryCalls == 0);

  loka::app::Fragment overflowParent;
  typedef loka::app::ForBuilder<
      ForItem,
      CountingTextFactory,
      loka::dsl::Expr<int, loka::dsl::IndexExpr> > DefaultBuilder;
  DefaultBuilder builder = loka::app::For(1, items, factory);
  overflowParent << builder.key(loka::dsl::Const(65535));
  LOKA_VERIFY(overflowParent.childrenCount() == 0);
  LOKA_VERIFY(factoryCalls == 0);

  loka::app::Fragment underflowParent;
  underflowParent << builder.key(loka::dsl::Const(-1));
  LOKA_VERIFY(underflowParent.childrenCount() == 0);
  LOKA_VERIFY(factoryCalls == 0);
}

void testForFactoryCloneFailureLeavesParentUnchanged()
{
  loka::Vector<ForItem> items;
  items.push_back(ForItem(1, "one"));
  items.push_back(ForItem(2, "two"));
  items.push_back(ForItem(3, "three"));

  loka::app::Fragment parent;
  loka::app::TextDefinition stable = loka::app::Text("stable");
  parent << stable;
  loka::app::scene::NodeDefinitionBase *stableClone = parent.childrenHead();
  parent << loka::app::For(800, items, FailingCloneFactory(1));

  LOKA_VERIFY(parent.childrenCount() == 1);
  LOKA_VERIFY(parent.childrenHead() == stableClone);
  LOKA_VERIFY(stableClone && !stableClone->nextInComposition);
}

void testUniqueTaggedSiblingListRejectsAnonymousSibling()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  LOKA_VERIFY(child >= 0);
  if (child == 0)
  {
    loka::app::Fragment root;
    root << loka::app::Section(900) << loka::app::Text("anonymous");
    loka::app::scene::NodeComposition composition;
    composition.declare(root);
    composition.assignCompositionSeatSlots();
    _exit(0);
  }

  int status = 0;
  LOKA_VERIFY(waitpid(child, &status, 0) == child);
  LOKA_VERIFY(WIFSIGNALED(status));
  LOKA_VERIFY(WTERMSIG(status) == SIGABRT);
#endif
}

void testForDerivedKeysRetainItemSeatAcrossRemoval()
{
  loka::Vector<ForItem> beforeItems;
  beforeItems.push_back(ForItem(1, "a"));
  beforeItems.push_back(ForItem(2, "b"));
  beforeItems.push_back(ForItem(3, "c"));
  loka::Vector<ForItem> afterItems;
  afterItems.push_back(ForItem(1, "a"));
  afterItems.push_back(ForItem(3, "c"));

  loka::app::scene::NodeCompositionSnapshot derivedBefore;
  loka::app::scene::NodeCompositionSnapshot derivedAfter;
  captureForSnapshot(beforeItems, true, derivedBefore);
  captureForSnapshot(afterItems, true, derivedAfter);

  loka::app::scene::NodeCompositionDiff derivedDiff;
  LOKA_VERIFY(loka::app::scene::buildNodeCompositionSnapshotDiffByTag(
      derivedBefore, derivedAfter, derivedDiff));
  loka::app::scene::NodeCompositionDiff::Entry *derivedC =
      findDiffEntry(derivedDiff, 103);
  LOKA_VERIFY(derivedC);
  LOKA_VERIFY(derivedC->action ==
              loka::app::scene::NodeCompositionDiff::ACTION_RETAIN);
  LOKA_VERIFY(derivedC->previousIndex == 2 && derivedC->currentIndex == 1);

  loka::app::scene::NodeCompositionSnapshot indexBefore;
  loka::app::scene::NodeCompositionSnapshot indexAfter;
  captureForSnapshot(beforeItems, false, indexBefore);
  captureForSnapshot(afterItems, false, indexAfter);

  loka::app::scene::NodeCompositionDiff indexDiff;
  LOKA_VERIFY(loka::app::scene::buildNodeCompositionSnapshotDiffByTag(
      indexBefore, indexAfter, indexDiff));
  loka::app::scene::NodeCompositionDiff::Entry *oldCSeat =
      findDiffEntry(indexDiff, 102);
  LOKA_VERIFY(oldCSeat);
  LOKA_VERIFY(oldCSeat->action ==
              loka::app::scene::NodeCompositionDiff::ACTION_RETIRE);
  loka::app::scene::NodeCompositionDiff::Entry *shiftedCSeat =
      findDiffEntry(indexDiff, 101);
  LOKA_VERIFY(shiftedCSeat);
  LOKA_VERIFY(shiftedCSeat->action ==
              loka::app::scene::NodeCompositionDiff::ACTION_RETAIN);
  LOKA_VERIFY(shiftedCSeat->previousIndex == 1 &&
              shiftedCSeat->currentIndex == 1);
}

void testForVectorBuilderReadsCurrentContentsAtAppend()
{
  loka::Vector<ForItem> items;
  items.push_back(ForItem(1, "one"));

  ForTextFactory factory;
  typedef loka::app::ForBuilder<
      ForItem,
      ForTextFactory,
      loka::dsl::Expr<int, loka::dsl::IndexExpr> > DefaultBuilder;
  DefaultBuilder builder = loka::app::For(1000, items, factory);

  items.push_back(ForItem(2, "two"));
  items.push_back(ForItem(3, "three"));

  loka::app::Fragment generated;
  generated << builder;

  loka::app::Fragment expected;
  for (std::size_t i = 0; i < items.size(); ++i)
  {
    loka::app::Section section(
        static_cast<loka::app::scene::NodeTag>(1000 + i));
    section << loka::app::Text(items[i].label);
    expected << section;
  }
  assertEquivalentDefinitionTree(&expected, &generated);
}

void testForWindowBuildsHandWrittenSubrange()
{
  loka::Vector<ForItem> items;
  items.push_back(ForItem(0, "zero"));
  items.push_back(ForItem(1, "one"));
  items.push_back(ForItem(2, "two"));
  items.push_back(ForItem(3, "three"));
  items.push_back(ForItem(4, "four"));
  items.push_back(ForItem(5, "five"));

  std::size_t indexes[3] = {0, 0, 0};
  std::size_t indexCount = 0;
  loka::app::Fragment generated;
  generated << loka::app::For(
                   1300, items, RecordingIndexTextFactory(indexes, &indexCount))
                   .window(2, 3);

  loka::app::Fragment expected;
  for (std::size_t i = 2; i < 5; ++i)
  {
    loka::app::Section section(
        static_cast<loka::app::scene::NodeTag>(1300 + i));
    section << loka::app::Text(items[i].label);
    expected << section;
  }

  assertEquivalentDefinitionTree(&expected, &generated);
  LOKA_VERIFY(indexCount == 3);
  LOKA_VERIFY(indexes[0] == 2);
  LOKA_VERIFY(indexes[1] == 3);
  LOKA_VERIFY(indexes[2] == 4);
}

void testForWindowClampsToLastValidStart()
{
  loka::Vector<ForItem> items;
  items.push_back(ForItem(0, "zero"));
  items.push_back(ForItem(1, "one"));
  items.push_back(ForItem(2, "two"));
  items.push_back(ForItem(3, "three"));
  items.push_back(ForItem(4, "four"));
  items.push_back(ForItem(5, "five"));

  loka::app::Fragment nearOvershoot;
  nearOvershoot << loka::app::For(1400, items, ForTextFactory()).window(5, 4);
  LOKA_VERIFY(nearOvershoot.childrenCount() == 4);
  LOKA_VERIFY(nearOvershoot.childrenHead()->nodeTag() == 1402);

  loka::app::Fragment farOvershoot;
  farOvershoot << loka::app::For(1400, items, ForTextFactory()).window(100, 4);
  LOKA_VERIFY(farOvershoot.childrenCount() == 4);
  LOKA_VERIFY(farOvershoot.childrenHead()->nodeTag() == 1402);

  loka::app::Fragment negativeFirst;
  negativeFirst << loka::app::For(1400, items, ForTextFactory()).window(-3, 4);
  LOKA_VERIFY(negativeFirst.childrenCount() == 4);
  LOKA_VERIFY(negativeFirst.childrenHead()->nodeTag() == 1400);

  loka::app::Fragment oversizedCount;
  oversizedCount << loka::app::For(1400, items, ForTextFactory()).window(3, 10);
  LOKA_VERIFY(oversizedCount.childrenCount() == 6);
  LOKA_VERIFY(oversizedCount.childrenHead()->nodeTag() == 1400);

  loka::app::Fragment zeroCount;
  zeroCount << loka::app::For(1400, items, ForTextFactory()).window(3, 0);
  LOKA_VERIFY(zeroCount.childrenCount() == 0);

  loka::app::Fragment negativeCount;
  negativeCount << loka::app::For(1400, items, ForTextFactory()).window(3, -2);
  LOKA_VERIFY(negativeCount.childrenCount() == 0);

  loka::Vector<ForItem> emptyItems;
  loka::app::Fragment empty;
  empty << loka::app::For(1400, emptyItems, ForTextFactory()).window(3, 4);
  LOKA_VERIFY(empty.childrenCount() == 0);

  int factoryCalls = 0;
  loka::app::Fragment invalidBase;
  invalidBase << loka::app::For(0, items, CountingTextFactory(&factoryCalls))
                     .window(1, 2);
  LOKA_VERIFY(invalidBase.childrenCount() == 0);
  LOKA_VERIFY(factoryCalls == 0);
}

void testForWindowSurvivesKeyChainInBothOrders()
{
  loka::Vector<ForItem> items;
  items.push_back(ForItem(10, "ten"));
  items.push_back(ForItem(20, "twenty"));
  items.push_back(ForItem(30, "thirty"));
  items.push_back(ForItem(40, "forty"));

  typedef loka::app::ForBuilder<
      ForItem,
      ForTextFactory,
      loka::dsl::Expr<int, loka::dsl::IndexExpr> > DefaultBuilder;
  ForTextFactory factory;
  DefaultBuilder builder = loka::app::For(1500, items, factory);

  loka::app::Fragment windowThenKey;
  windowThenKey << builder.window(1, 2).key(
      builder.slot.member<int, &ForItem::id>());
  loka::app::Fragment keyThenWindow;
  keyThenWindow << builder.key(builder.slot.member<int, &ForItem::id>())
                       .window(1, 2);
  assertEquivalentDefinitionTree(&windowThenKey, &keyThenWindow);
  LOKA_VERIFY(windowThenKey.childrenCount() == 2);

  const ForItem arrayItems[] = {
      ForItem(10, "ten"),
      ForItem(20, "twenty"),
      ForItem(30, "thirty"),
      ForItem(40, "forty")};
  DefaultBuilder arrayBuilder = loka::app::For(1500, arrayItems, factory);
  loka::app::Fragment arrayWindowThenKey;
  arrayWindowThenKey << arrayBuilder.window(1, 2).key(
      arrayBuilder.slot.member<int, &ForItem::id>());
  loka::app::Fragment arrayKeyThenWindow;
  arrayKeyThenWindow <<
      arrayBuilder.key(arrayBuilder.slot.member<int, &ForItem::id>())
          .window(1, 2);
  assertEquivalentDefinitionTree(&arrayWindowThenKey, &arrayKeyThenWindow);
  LOKA_VERIFY(arrayWindowThenKey.childrenCount() == 2);
}

void testForWindowSlideRetainsOverlappingSeatsInOrder()
{
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<ForWindowBoundaryNode>()));
  scene.mount(&platform);
  scene.updateAttached(true);

  ForWindowBoundaryNode *root = static_cast<ForWindowBoundaryNode *>(
      loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(root != 0);
  loka::app::BoundarySectionNode *before[4] = {
      root->section(1200),
      root->section(1201),
      root->section(1202),
      root->section(1203)};
  LOKA_VERIFY(before[0] != 0);
  LOKA_VERIFY(before[1] != 0);
  LOKA_VERIFY(before[2] != 0);
  LOKA_VERIFY(before[3] != 0);

  root->setFirst(1);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());

  loka::app::BoundarySectionNode *removed = root->section(1200);
  loka::app::BoundarySectionNode *retainedOne = root->section(1201);
  loka::app::BoundarySectionNode *retainedTwo = root->section(1202);
  loka::app::BoundarySectionNode *retainedThree = root->section(1203);
  LOKA_VERIFY(removed == 0);
  LOKA_VERIFY(retainedOne == before[1]);
  LOKA_VERIFY(retainedTwo == before[2]);
  LOKA_VERIFY(retainedThree == before[3]);
  loka::app::BoundarySectionNode *fresh = root->section(1204);
  LOKA_VERIFY(fresh != 0);
  LOKA_VERIFY(fresh != before[0]);
  LOKA_VERIFY(fresh != before[1]);
  LOKA_VERIFY(fresh != before[2]);
  LOKA_VERIFY(fresh != before[3]);

  loka::app::ColumnNode *column = root->column();
  LOKA_VERIFY(column != 0);
  loka::app::scene::Node *child = column->childrenHead();
  for (std::size_t i = 0; i < 4; ++i)
  {
    LOKA_VERIFY(child != 0);
    LOKA_VERIFY(child->nodeTag() == 1201 + i);
    child = child->nextInComposition;
  }
  LOKA_VERIFY(child == 0);
}

void testForWindowRejectsDuplicateKeysOutsideWindow()
{
  // Keys `[7, 8, 7]` with `window(0, 2)` expand only two rows, but the seat
  // identity domain is the whole item list: sliding to `window(1, 2)` would
  // otherwise hand the first item's Section (state included) to the third.
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  LOKA_VERIFY(child >= 0);
  if (child == 0)
  {
    loka::Vector<ForItem> items;
    items.push_back(ForItem(7, "first seven"));
    items.push_back(ForItem(8, "eight"));
    items.push_back(ForItem(7, "second seven"));
    typedef loka::app::ForBuilder<
        ForItem,
        ForTextFactory,
        loka::dsl::Expr<int, loka::dsl::IndexExpr> > DefaultBuilder;
    ForTextFactory factory;
    DefaultBuilder builder = loka::app::For(1600, items, factory);
    loka::app::Fragment parent;
    parent << builder.key(builder.slot.member<int, &ForItem::id>()).window(0, 2);
    _exit(0);
  }

  int status = 0;
  LOKA_VERIFY(waitpid(child, &status, 0) == child);
  LOKA_VERIFY(WIFSIGNALED(status));
  LOKA_VERIFY(WTERMSIG(status) == SIGABRT);
#endif
}
