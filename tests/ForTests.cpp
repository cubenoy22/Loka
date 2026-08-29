#include "ForTests.hpp"
#include "support/TestVerify.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "app/nodes/Text.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/For.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/composition/NodeCompositionCompare.hpp"
#include "app/scene/composition/NodeCompositionSnapshot.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "core/State.hpp"
#include "core/Vector.hpp"

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

  void assertEquivalentDefinitionTree(
      const loka::app::scene::NodeDefinitionBase *expected,
      const loka::app::scene::NodeDefinitionBase *actual)
  {
    assert(expected && actual);
    assert(expected->nodeTag() == actual->nodeTag());
    assert(expected->propsBase() && actual->propsBase());
    assert(expected->propsBase()->propsTypeId() ==
           actual->propsBase()->propsTypeId());
    assert(expected->hasEquivalentProps(*actual));

    loka::app::scene::INestableDefinition *expectedNestable =
        const_cast<loka::app::scene::NodeDefinitionBase *>(expected)
            ->asNestableDefinition();
    loka::app::scene::INestableDefinition *actualNestable =
        const_cast<loka::app::scene::NodeDefinitionBase *>(actual)
            ->asNestableDefinition();
    assert((expectedNestable != 0) == (actualNestable != 0));
    if (!expectedNestable)
    {
      return;
    }

    assert(expectedNestable->childrenCount() == actualNestable->childrenCount());
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
    assert(!expectedChild && !actualChild);
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
      DefaultBuilder builder = loka::app::For(100, items, ForTextFactory());
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
  assert(show.childrenCount() == 3);

  loka::app::PolicyScopeDefinition policy;
  policy << loka::app::For(600, items);
  loka::app::scene::INestableDefinition *policyContent =
      policy.scopedBranchDefinition()->asNestableDefinition();
  assert(policyContent && policyContent->childrenCount() == 3);
}

void testForRejectsDuplicateKeysWithinBatch()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  assert(child >= 0);
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
    DefaultBuilder builder = loka::app::For(700, items, ForTextFactory());
    parent << builder.key(loka::dsl::Const(0));
    _exit(0);
  }

  int status = 0;
  LOKA_VERIFY(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGABRT);
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
  assert(zeroBaseParent.childrenCount() == 0);
  assert(factoryCalls == 0);

  loka::app::Fragment overflowParent;
  typedef loka::app::ForBuilder<
      ForItem,
      CountingTextFactory,
      loka::dsl::Expr<int, loka::dsl::IndexExpr> > DefaultBuilder;
  DefaultBuilder builder = loka::app::For(1, items, factory);
  overflowParent << builder.key(loka::dsl::Const(65535));
  assert(overflowParent.childrenCount() == 0);
  assert(factoryCalls == 0);

  loka::app::Fragment underflowParent;
  underflowParent << builder.key(loka::dsl::Const(-1));
  assert(underflowParent.childrenCount() == 0);
  assert(factoryCalls == 0);
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

  assert(parent.childrenCount() == 1);
  assert(parent.childrenHead() == stableClone);
  assert(stableClone && !stableClone->nextInComposition);
}

void testUniqueTaggedSiblingListRejectsAnonymousSibling()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  assert(child >= 0);
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
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGABRT);
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
  assert(derivedC);
  assert(derivedC->action ==
         loka::app::scene::NodeCompositionDiff::ACTION_RETAIN);
  assert(derivedC->previousIndex == 2 && derivedC->currentIndex == 1);

  loka::app::scene::NodeCompositionSnapshot indexBefore;
  loka::app::scene::NodeCompositionSnapshot indexAfter;
  captureForSnapshot(beforeItems, false, indexBefore);
  captureForSnapshot(afterItems, false, indexAfter);

  loka::app::scene::NodeCompositionDiff indexDiff;
  LOKA_VERIFY(loka::app::scene::buildNodeCompositionSnapshotDiffByTag(
      indexBefore, indexAfter, indexDiff));
  loka::app::scene::NodeCompositionDiff::Entry *oldCSeat =
      findDiffEntry(indexDiff, 102);
  assert(oldCSeat);
  assert(oldCSeat->action ==
         loka::app::scene::NodeCompositionDiff::ACTION_RETIRE);
  loka::app::scene::NodeCompositionDiff::Entry *shiftedCSeat =
      findDiffEntry(indexDiff, 101);
  assert(shiftedCSeat);
  assert(shiftedCSeat->action ==
         loka::app::scene::NodeCompositionDiff::ACTION_RETAIN);
  assert(shiftedCSeat->previousIndex == 1 && shiftedCSeat->currentIndex == 1);
}
