#include "NullPlatformContractTests.hpp"

#include <cassert>

#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/Conditional.hpp"
#include "core/State.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/NullWindow.hpp"
#include "support/RecomposingBoundary.hpp"

namespace
{
  template <class NodeT, class PropsT>
  class RecomposingContractBoundaryNode : public loka::app::scene::BoundaryNodeFor<NodeT>
  {
  public:
    explicit RecomposingContractBoundaryNode(const PropsT &props)
        : loka::app::scene::BoundaryNodeFor<NodeT>(props)
    {
    }

  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::BoundaryNodeFor<NodeT> BaseType;
      if (event != loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        BaseType::composeWithContext(context, event);
        return;
      }

      loka::app::scene::NodeComposition &composition = this->beginComposition(context);
      {
        loka::app::scene::NodeComposition::CompositionScope scope(composition);
        this->composeNode(composition);
      }
      this->captureCurrentCompositionSnapshot();
      this->rebuildCurrentCompositionDiff();
      std::vector<loka::app::scene::Node *> retainedChildren;
      if (!this->rebuildCompositionChildrenFromCurrentSnapshot(context, retainedChildren))
      {
        return;
      }
      this->promoteCurrentCompositionSnapshot();
      for (std::size_t i = 0; i < retainedChildren.size(); ++i)
      {
        if (retainedChildren[i])
        {
          this->composeSubtree(retainedChildren[i], context, event, this);
        }
      }
    }
  };

  loka::core::MutableState<bool> *g_toggleVisible = 0;
  loka::app::scene::NativeLifetimeHint g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;

  class ToggleControlBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ToggleControlBoundaryNode> ToggleControlBoundaryProps;

  class ToggleControlBoundaryNode
      : public RecomposingContractBoundaryNode<ToggleControlBoundaryNode, ToggleControlBoundaryProps>
  {
  public:
    explicit ToggleControlBoundaryNode(const ToggleControlBoundaryProps &props)
        : RecomposingContractBoundaryNode<ToggleControlBoundaryNode, ToggleControlBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_toggleVisible)
      {
        registrar.markDirtyOnChange(g_toggleVisible, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_toggleVisible && g_toggleVisible->get())
      {
        loka::app::ButtonDefinition button("toggle");
        button.lifetimeHint(g_toggleHint);
        root << button;
      }
      composition.declare(root);
    }
  };

  loka::core::MutableState<bool> *g_retainedCondition = 0;

  class RetainedButtonBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RetainedButtonBoundaryNode> RetainedButtonBoundaryProps;

  class RetainedButtonBoundaryNode : public loka::app::scene::BoundaryNodeFor<RetainedButtonBoundaryNode>
  {
  public:
    explicit RetainedButtonBoundaryNode(const RetainedButtonBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<RetainedButtonBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::ButtonDefinition button("retained");
      loka::app::FragmentDefinition empty;
      loka::app::scene::ConditionalDefinition conditional(
          (loka::app::scene::ConditionalProps(g_retainedCondition, &button, &empty)));
      loka::app::FragmentDefinition root;
      root << conditional;
      composition.declare(root);
    }
  };

  loka::core::MutableState<bool> *g_hiddenAncestorVisible = 0;
  loka::core::MutableState<bool> *g_hiddenInnerCondition = 0;

  class HiddenSwapBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HiddenSwapBoundaryNode> HiddenSwapBoundaryProps;

  class HiddenSwapBoundaryNode : public loka::app::scene::BoundaryNodeFor<HiddenSwapBoundaryNode>
  {
  public:
    explicit HiddenSwapBoundaryNode(const HiddenSwapBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<HiddenSwapBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::ButtonDefinition button("inner-button");
      loka::app::EditTextDefinition editText;
      loka::app::scene::ConditionalDefinition inner(
          (loka::app::scene::ConditionalProps(g_hiddenInnerCondition, &button, &editText)));
      loka::app::ShowDefinition outer(g_hiddenAncestorVisible);
      outer << inner;
      loka::app::FragmentDefinition root;
      root << outer;
      composition.declare(root);
    }
  };

  loka::core::MutableState<int> *g_recipeMode = 0;

  class RecipeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RecipeBoundaryNode> RecipeBoundaryProps;

  class RecipeBoundaryNode
      : public RecomposingContractBoundaryNode<RecipeBoundaryNode, RecipeBoundaryProps>
  {
  public:
    explicit RecipeBoundaryNode(const RecipeBoundaryProps &props)
        : RecomposingContractBoundaryNode<RecipeBoundaryNode, RecipeBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_recipeMode)
      {
        registrar.markDirtyOnChange(g_recipeMode, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_recipeMode && g_recipeMode->get() == 1)
      {
        root << loka::app::Button("recipe-button");
      }
      else if (g_recipeMode && g_recipeMode->get() == 2)
      {
        root << loka::app::EditText();
      }
      composition.declare(root);
    }
  };

  loka::core::MutableState<bool> *g_multipleVisible = 0;

  class MultipleButtonBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<MultipleButtonBoundaryNode> MultipleButtonBoundaryProps;

  class MultipleButtonBoundaryNode
      : public RecomposingContractBoundaryNode<MultipleButtonBoundaryNode, MultipleButtonBoundaryProps>
  {
  public:
    explicit MultipleButtonBoundaryNode(const MultipleButtonBoundaryProps &props)
        : RecomposingContractBoundaryNode<MultipleButtonBoundaryNode, MultipleButtonBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_multipleVisible)
      {
        registrar.markDirtyOnChange(g_multipleVisible, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition controls;
      if (g_multipleVisible && g_multipleVisible->get())
      {
        controls << loka::app::Button("first") << loka::app::Button("second");
      }
      composition.declare(controls);
    }
  };
  loka::core::MutableState<bool> *g_parkedSubtreeVisible = 0;
  loka::core::MutableState<bool> *g_parkedInnerCondition = 0;

  // Non-recomposing inner boundary: the Conditional (and its parked branch)
  // persists across the outer boundary's diffs, so the retire exercises the
  // deferred-drain path instead of an immediate replacement.
  class ParkedBranchInnerBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ParkedBranchInnerBoundaryNode> ParkedBranchInnerBoundaryProps;

  class ParkedBranchInnerBoundaryNode : public loka::app::scene::BoundaryNodeFor<ParkedBranchInnerBoundaryNode>
  {
  public:
    explicit ParkedBranchInnerBoundaryNode(const ParkedBranchInnerBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<ParkedBranchInnerBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::ButtonDefinition buttonA("parked-a");
      loka::app::ButtonDefinition buttonB("parked-b");
      loka::app::scene::ConditionalDefinition conditional(
          (loka::app::scene::ConditionalProps(g_parkedInnerCondition, &buttonA, &buttonB)));
      loka::app::FragmentDefinition root;
      root << conditional;
      composition.declare(root);
    }
  };

  class ParkedBranchRetireBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ParkedBranchRetireBoundaryNode> ParkedBranchRetireBoundaryProps;

  class ParkedBranchRetireBoundaryNode
      : public RecomposingContractBoundaryNode<ParkedBranchRetireBoundaryNode, ParkedBranchRetireBoundaryProps>
  {
  public:
    explicit ParkedBranchRetireBoundaryNode(const ParkedBranchRetireBoundaryProps &props)
        : RecomposingContractBoundaryNode<ParkedBranchRetireBoundaryNode, ParkedBranchRetireBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_parkedSubtreeVisible)
      {
        registrar.markDirtyOnChange(g_parkedSubtreeVisible, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_parkedSubtreeVisible && g_parkedSubtreeVisible->get())
      {
        root << loka::app::scene::Boundary<ParkedBranchInnerBoundaryNode>();
      }
      composition.declare(root);
    }
  };

  struct NativeContextCallCounts
  {
    explicit NativeContextCallCounts(const NullScenePlatformController &platform)
        : created(platform.createdCount()),
          disposed(platform.disposedCount()),
          backPointerCleared(platform.teardownCounters().backPointerCleared),
          rowRemoved(platform.teardownCounters().rowRemoved),
          handedToPool(platform.teardownCounters().handedToPool),
          shown(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN)),
          hidden(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN))
    {
    }

    bool operator==(const NativeContextCallCounts &other) const
    {
      return this->created == other.created &&
             this->disposed == other.disposed &&
             this->backPointerCleared == other.backPointerCleared &&
             this->rowRemoved == other.rowRemoved &&
             this->handedToPool == other.handedToPool &&
             this->shown == other.shown &&
             this->hidden == other.hidden;
    }

    unsigned long created;
    unsigned long disposed;
    unsigned long backPointerCleared;
    unsigned long rowRemoved;
    unsigned long handedToPool;
    unsigned long shown;
    unsigned long hidden;
  };

  void assertExpectedRedForT3(bool observableOutcome)
  {
    assert(!observableOutcome &&
           "expected-red contract pin turned green; T3 must promote this outcome assertion");
  }

  void assertExpectedRedForT4(bool observableOutcome)
  {
    assert(!observableOutcome &&
           "expected-red contract pin turned green; T4 must promote this outcome assertion");
  }

  struct FactTransition
  {
    FactTransition(loka::app::scene::NodeLifecycleFact previousFact,
                   loka::app::scene::NodeLifecycleFact nextFact)
        : previous(previousFact),
          next(nextFact)
    {
    }

    loka::app::scene::NodeLifecycleFact previous;
    loka::app::scene::NodeLifecycleFact next;
  };

  struct ParkedFactRecord
  {
    ParkedFactRecord()
        : constructionCount(0),
          attachReads(0),
          transitions()
    {
    }

    int constructionCount;
    int attachReads;
    std::vector<FactTransition> transitions;
  };

  class ParkedFactContext : public loka::app::scene::NodeContext
  {
  public:
    explicit ParkedFactContext(ParkedFactRecord *record)
        : record_(record)
    {
    }

    void readFactOnAttach()
    {
      if (this->record_ && this->owner() &&
          this->owner()->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
      {
        ++this->record_->attachReads;
      }
    }

    virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                               loka::app::scene::NodeLifecycleFact next)
    {
      if (this->record_)
      {
        this->record_->transitions.push_back(FactTransition(previous, next));
      }
    }

  private:
    ParkedFactRecord *record_;
  };

  class ParkedFactNode;

  struct ParkedFactTypeTag
  {
  };

  struct ParkedFactProps : public loka::app::scene::NodePropsBase<ParkedFactProps>
  {
    typedef ParkedFactTypeTag TypeTag;
    typedef ParkedFactNode NodeType;

    explicit ParkedFactProps(ParkedFactRecord *factRecord = 0)
        : record(factRecord)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }

    ParkedFactRecord *record;
  };

  class ParkedFactNode : public loka::app::scene::Node
  {
  public:
    typedef ParkedFactTypeTag TypeTag;

    explicit ParkedFactNode(const ParkedFactProps &props)
        : props(props)
    {
      if (this->props.record)
      {
        ++this->props.record->constructionCount;
      }
      ParkedFactContext *context = new ParkedFactContext(props.record);
      this->setContext(context);
      context->readFactOnAttach();
    }

    ParkedFactProps props;
  };

  typedef loka::app::scene::NodeDefinition<ParkedFactProps, ParkedFactNode> ParkedFactDefinition;

  bool recordedTransitionTo(const ParkedFactRecord &record,
                            loka::app::scene::NodeLifecycleFact fact,
                            std::size_t begin)
  {
    for (std::size_t i = begin; i < record.transitions.size(); ++i)
    {
      if (record.transitions[i].next == fact)
      {
        return true;
      }
    }
    return false;
  }

  void assertParkedTransitionTable(const ParkedFactRecord &record)
  {
    assert(record.constructionCount == 1);
    assert(record.attachReads == 1);
    assert(record.transitions.size() == 1);
    assert(record.transitions[0].previous == loka::app::scene::NODE_FACT_ATTACHED);
    assert(record.transitions[0].next == loka::app::scene::NODE_FACT_DETACHED_RETAINED);
  }

  template <class NodeT, class PropsT>
  class PropsRecomposingBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<NodeT, PropsT>
  {
  public:
    explicit PropsRecomposingBoundaryNode(const PropsT &props)
        : SceneTestSupport::RecomposingBoundaryNode<NodeT, PropsT>(props)
    {
    }

  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      if (event == loka::app::scene::COMPOSE_EVENT_UPDATE &&
          !(context.dirtyFlags() & loka::app::scene::NODE_DIRTY_PROPS))
      {
        typedef loka::app::scene::BoundaryNodeFor<NodeT> BaseType;
        BaseType::composeWithContext(context, event);
        return;
      }
      SceneTestSupport::RecomposingBoundaryNode<NodeT, PropsT>::composeWithContext(context, event);
    }
  };

  loka::core::MutableState<bool> *g_seatCondition = 0;
  loka::core::MutableState<int> *g_seatUnrelatedState = 0;
  loka::core::MutableState<loka::core::String> *g_seatDraft = 0;
  ParkedFactRecord *g_seatProbeRecord = 0;
  ParkedFactRecord *g_seatOldBranchRecord = 0;
  ParkedFactRecord *g_seatCurrentBranchRecord = 0;

  class ConditionalSeatBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ConditionalSeatBoundaryNode> ConditionalSeatBoundaryProps;
  class ConditionalSeatBoundaryNode
      : public PropsRecomposingBoundaryNode<ConditionalSeatBoundaryNode,
                                            ConditionalSeatBoundaryProps>
  {
  public:
    explicit ConditionalSeatBoundaryNode(const ConditionalSeatBoundaryProps &props)
        : PropsRecomposingBoundaryNode<ConditionalSeatBoundaryNode,
                                       ConditionalSeatBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_seatUnrelatedState)
      {
        registrar.markDirtyOnChange(g_seatUnrelatedState, loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::ButtonDefinition button("seat-active");
      ParkedFactRecord *probeRecord = g_seatProbeRecord;
      if (g_seatCurrentBranchRecord)
      {
        probeRecord = g_seatUnrelatedState && g_seatUnrelatedState->get() != 0
                          ? g_seatCurrentBranchRecord
                          : g_seatOldBranchRecord;
      }
      ParkedFactDefinition probe((ParkedFactProps(probeRecord)));
      loka::app::EditTextDefinition editText(g_seatDraft);
      loka::app::FragmentDefinition parkedDraft;
      parkedDraft << probe << editText;
      loka::app::scene::ConditionalDefinition conditional(
          (loka::app::scene::ConditionalProps(g_seatCondition, &button, &parkedDraft)));
      loka::app::FragmentDefinition root;
      root << conditional;
      composition.declare(root);
    }
  };

  class ConditionalSeatHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ConditionalSeatHarnessBoundaryNode>
      ConditionalSeatHarnessBoundaryProps;

  class ConditionalSeatHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<ConditionalSeatHarnessBoundaryNode>
  {
  public:
    explicit ConditionalSeatHarnessBoundaryNode(const ConditionalSeatHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<ConditionalSeatHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::scene::Boundary<ConditionalSeatBoundaryNode>());
    }
  };

  struct ConditionalContentInputs
  {
    ConditionalContentInputs(loka::core::MutableState<bool> *conditionState,
                             loka::core::MutableState<int> *activeRevisionState,
                             loka::core::MutableState<int> *parkedRevisionState,
                             ParkedFactRecord *activeRecord,
                             ParkedFactRecord *parkedRecord)
        : condition(conditionState),
          activeRevision(activeRevisionState),
          parkedRevision(parkedRevisionState),
          activeProbe(activeRecord),
          parkedProbe(parkedRecord)
    {
    }

    loka::core::MutableState<bool> *condition;
    loka::core::MutableState<int> *activeRevision;
    loka::core::MutableState<int> *parkedRevision;
    ParkedFactRecord *activeProbe;
    ParkedFactRecord *parkedProbe;
  };

  ConditionalContentInputs *g_contentInputs = 0;

  class ConditionalContentBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ConditionalContentBoundaryNode>
      ConditionalContentBoundaryProps;
  class ConditionalContentBoundaryNode
      : public PropsRecomposingBoundaryNode<ConditionalContentBoundaryNode,
                                            ConditionalContentBoundaryProps>
  {
  public:
    explicit ConditionalContentBoundaryNode(const ConditionalContentBoundaryProps &props)
        : PropsRecomposingBoundaryNode<ConditionalContentBoundaryNode,
                                       ConditionalContentBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_contentInputs && g_contentInputs->activeRevision)
      {
        registrar.markDirtyOnChange(g_contentInputs->activeRevision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
      if (g_contentInputs && g_contentInputs->parkedRevision)
      {
        registrar.markDirtyOnChange(g_contentInputs->parkedRevision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      const bool activeContentChanged =
          g_contentInputs && g_contentInputs->activeRevision &&
          g_contentInputs->activeRevision->get() != 0;
      const loka::app::scene::NativeLifetimeHint activeContentHint =
          activeContentChanged
              ? loka::app::scene::NATIVE_HINT_DESIRE_STAY
              : loka::app::scene::NATIVE_HINT_DEFAULT;
      const bool parkedContentChanged =
          g_contentInputs && g_contentInputs->parkedRevision &&
          g_contentInputs->parkedRevision->get() != 0;
      const loka::app::scene::NativeLifetimeHint parkedContentHint =
          parkedContentChanged
              ? loka::app::scene::NATIVE_HINT_DESIRE_STAY
              : loka::app::scene::NATIVE_HINT_DEFAULT;
      ParkedFactDefinition activeProbe(
          (ParkedFactProps(g_contentInputs ? g_contentInputs->activeProbe : 0)));
      loka::app::ButtonDefinition activeControl("active-content");
      activeControl.lifetimeHint(activeContentHint);
      loka::app::FragmentDefinition activeBranch;
      activeBranch << activeProbe << activeControl;
      ParkedFactDefinition parkedProbe(
          (ParkedFactProps(g_contentInputs ? g_contentInputs->parkedProbe : 0)));
      loka::app::EditTextDefinition parkedControl;
      parkedControl.lifetimeHint(parkedContentHint);
      loka::app::FragmentDefinition parkedBranch;
      parkedBranch << parkedProbe << parkedControl;
      loka::app::scene::ConditionalDefinition conditional(
          (loka::app::scene::ConditionalProps(
              g_contentInputs ? g_contentInputs->condition : 0,
              &activeBranch,
              &parkedBranch)));
      loka::app::FragmentDefinition root;
      root << conditional;
      composition.declare(root);
    }
  };

  class ConditionalContentHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ConditionalContentHarnessBoundaryNode>
      ConditionalContentHarnessBoundaryProps;
  class ConditionalContentHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<ConditionalContentHarnessBoundaryNode>
  {
  public:
    explicit ConditionalContentHarnessBoundaryNode(
        const ConditionalContentHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<ConditionalContentHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::scene::Boundary<ConditionalContentBoundaryNode>());
    }
  };

  class TaggedConditionalSeatBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<TaggedConditionalSeatBoundaryNode>
      TaggedConditionalSeatBoundaryProps;
  class TaggedConditionalSeatBoundaryNode
      : public PropsRecomposingBoundaryNode<TaggedConditionalSeatBoundaryNode,
                                            TaggedConditionalSeatBoundaryProps>
  {
  public:
    explicit TaggedConditionalSeatBoundaryNode(const TaggedConditionalSeatBoundaryProps &props)
        : PropsRecomposingBoundaryNode<TaggedConditionalSeatBoundaryNode,
                                       TaggedConditionalSeatBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_seatUnrelatedState)
      {
        registrar.markDirtyOnChange(g_seatUnrelatedState, loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::ButtonDefinition leading("tagged-leading");
      leading.tag(101);
      loka::app::ButtonDefinition active("tagged-seat-active");
      ParkedFactDefinition probe((ParkedFactProps(g_seatProbeRecord)));
      loka::app::EditTextDefinition parkedControl(g_seatDraft);
      loka::app::FragmentDefinition parkedBranch;
      parkedBranch << probe << parkedControl;
      loka::app::scene::ConditionalDefinition conditional(
          (loka::app::scene::ConditionalProps(g_seatCondition, &active, &parkedBranch)));
      conditional.setNodeTag(102);
      loka::app::EditTextDefinition trailing;
      trailing.tag(103);
      loka::app::FragmentDefinition root;
      // Per-tag retention currently requires every sibling to carry a unique tag.
      root << leading << conditional << trailing;
      composition.declare(root);
    }
  };

  class TaggedConditionalSeatHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<TaggedConditionalSeatHarnessBoundaryNode>
      TaggedConditionalSeatHarnessBoundaryProps;
  class TaggedConditionalSeatHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<TaggedConditionalSeatHarnessBoundaryNode>
  {
  public:
    explicit TaggedConditionalSeatHarnessBoundaryNode(
        const TaggedConditionalSeatHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<TaggedConditionalSeatHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::scene::Boundary<TaggedConditionalSeatBoundaryNode>());
    }
  };

  class DefinitionSourceProbeDefinition
      : public loka::app::scene::NodeDefinition<ParkedFactProps,
                                                ParkedFactNode>
  {
  public:
    typedef loka::app::scene::NodeDefinition<ParkedFactProps,
                                              ParkedFactNode>
        BaseType;

    DefinitionSourceProbeDefinition(ParkedFactRecord *liveRecord,
                                    ParkedFactRecord *expiredRecord)
        : BaseType(ParkedFactProps(liveRecord)),
          liveRecord_(liveRecord),
          expiredRecord_(expiredRecord)
    {
    }

    virtual ~DefinitionSourceProbeDefinition()
    {
      // If a retained seat reads this definition after its owner destroys it,
      // an allocator-preserved object reports the expired source record. A
      // poisoned/freed object instead hard-fails the same lifetime contract.
      this->props.record = this->expiredRecord_;
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      DefinitionSourceProbeDefinition *copy =
          new DefinitionSourceProbeDefinition(this->liveRecord_, this->expiredRecord_);
      if (copy)
      {
        copy->copyTestIdPolicyFrom(*this);
      }
      return copy;
    }

  private:
    ParkedFactRecord *liveRecord_;
    ParkedFactRecord *expiredRecord_;
  };

  struct TaggedPropsApplyInputs
  {
    explicit TaggedPropsApplyInputs(loka::core::State<bool> *conditionState)
        : condition(conditionState)
    {
    }

    loka::core::State<bool> *condition;
  };

  class TaggedPropsApplyConditionalDefinition
      : public loka::app::scene::ConditionalDefinition
  {
  public:
    TaggedPropsApplyConditionalDefinition(
        const loka::app::scene::ConditionalProps &props,
        TaggedPropsApplyInputs *inputs)
        : loka::app::scene::ConditionalDefinition(props),
          inputs_(inputs)
    {
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      // RootBoundaryWrapper clones the fixed scene definition each generation;
      // read the test-owned input here so tagged props become non-equivalent.
      TaggedPropsApplyConditionalDefinition *copy =
          new TaggedPropsApplyConditionalDefinition(
              loka::app::scene::ConditionalProps(
                  this->inputs_ ? this->inputs_->condition : 0,
                  this->ownedTrueDef,
                  this->ownedFalseDef),
              this->inputs_);
      if (copy)
      {
        copy->copyTestIdPolicyFrom(*this);
      }
      return copy;
    }

  private:
    TaggedPropsApplyInputs *inputs_;
  };

  loka::core::MutableState<bool> *g_deferredFlipCondition = 0;
  ParkedFactRecord *g_deferredTrueRecord = 0;
  ParkedFactRecord *g_deferredFalseRecord = 0;

  class DeferredFlipBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<DeferredFlipBoundaryNode> DeferredFlipBoundaryProps;
  class DeferredFlipBoundaryNode : public loka::app::scene::BoundaryNodeFor<DeferredFlipBoundaryNode>
  {
  public:
    explicit DeferredFlipBoundaryNode(const DeferredFlipBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<DeferredFlipBoundaryNode>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      ParkedFactDefinition trueProbe((ParkedFactProps(g_deferredTrueRecord)));
      loka::app::ButtonDefinition button("flip-true");
      loka::app::FragmentDefinition trueBranch;
      trueBranch << trueProbe << button;
      ParkedFactDefinition falseProbe((ParkedFactProps(g_deferredFalseRecord)));
      loka::app::EditTextDefinition editText;
      loka::app::FragmentDefinition falseBranch;
      falseBranch << falseProbe << editText;
      loka::app::scene::ConditionalDefinition conditional(
          (loka::app::scene::ConditionalProps(g_deferredFlipCondition, &trueBranch, &falseBranch)));
      loka::app::FragmentDefinition root;
      root << conditional;
      composition.declare(root);
    }
  };

  loka::core::MutableState<bool> *g_enumeratedSubtreeVisible = 0;
  loka::core::MutableState<bool> *g_enumeratedFirstCondition = 0;
  loka::core::MutableState<bool> *g_enumeratedSecondCondition = 0;
  ParkedFactRecord *g_enumeratedFirstRecord = 0;
  ParkedFactRecord *g_enumeratedSecondRecord = 0;
  class EnumeratedBranchesInnerBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<EnumeratedBranchesInnerBoundaryNode>
      EnumeratedBranchesInnerBoundaryProps;
  class EnumeratedBranchesInnerBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<EnumeratedBranchesInnerBoundaryNode>
  {
  public:
    explicit EnumeratedBranchesInnerBoundaryNode(const EnumeratedBranchesInnerBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<EnumeratedBranchesInnerBoundaryNode>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      ParkedFactDefinition first((ParkedFactProps(g_enumeratedFirstRecord)));
      ParkedFactDefinition second((ParkedFactProps(g_enumeratedSecondRecord)));
      loka::app::FragmentDefinition empty;
      loka::app::scene::ConditionalDefinition nested(
          (loka::app::scene::ConditionalProps(g_enumeratedSecondCondition, &empty, &second)));
      loka::app::FragmentDefinition parked;
      parked << first << nested;
      loka::app::scene::ConditionalDefinition outer(
          (loka::app::scene::ConditionalProps(g_enumeratedFirstCondition, &empty, &parked)));
      loka::app::FragmentDefinition root;
      root << outer;
      composition.declare(root);
    }
  };

  class EnumeratedBranchesRetireBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<EnumeratedBranchesRetireBoundaryNode>
      EnumeratedBranchesRetireBoundaryProps;

  class EnumeratedBranchesRetireBoundaryNode
      : public PropsRecomposingBoundaryNode<EnumeratedBranchesRetireBoundaryNode,
                                            EnumeratedBranchesRetireBoundaryProps>
  {
  public:
    explicit EnumeratedBranchesRetireBoundaryNode(const EnumeratedBranchesRetireBoundaryProps &props)
        : PropsRecomposingBoundaryNode<EnumeratedBranchesRetireBoundaryNode,
                                       EnumeratedBranchesRetireBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_enumeratedSubtreeVisible)
      {
        registrar.markDirtyOnChange(g_enumeratedSubtreeVisible, loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_enumeratedSubtreeVisible && g_enumeratedSubtreeVisible->get())
      {
        root << loka::app::scene::Boundary<EnumeratedBranchesInnerBoundaryNode>();
      }
      composition.declare(root);
    }
  };

  class EnumeratedBranchesHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<EnumeratedBranchesHarnessBoundaryNode>
      EnumeratedBranchesHarnessBoundaryProps;

  class EnumeratedBranchesHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<EnumeratedBranchesHarnessBoundaryNode>
  {
  public:
    explicit EnumeratedBranchesHarnessBoundaryNode(const EnumeratedBranchesHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<EnumeratedBranchesHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::scene::Boundary<EnumeratedBranchesRetireBoundaryNode>());
    }
  };

  void assertParkedRetirementTransitionTable(const ParkedFactRecord &record)
  {
    assert(record.constructionCount == 1);
    assert(record.attachReads == 1);
    assert(record.transitions.size() == 2);
    assert(record.transitions[0].previous == loka::app::scene::NODE_FACT_ATTACHED);
    assert(record.transitions[0].next == loka::app::scene::NODE_FACT_DETACHED_RETAINED);
    assert(record.transitions[1].previous == loka::app::scene::NODE_FACT_DETACHED_RETAINED);
    assert(record.transitions[1].next == loka::app::scene::NODE_FACT_RETIRED);
  }

  void requestChildPump(loka::app::scene::Scene &scene)
  {
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());
  }

  void mountAndAttach(loka::app::scene::Scene &scene, NullScenePlatformController &platform)
  {
    scene.mount(&platform);
    scene.updateAttached(true);
  }

  void assertDisposalsAreInsideSafePoints(const NullScenePlatformController &platform)
  {
    const std::vector<NullScenePlatformController::EventRecord> &events = platform.eventLog();
    int safePointDepth = 0;
    unsigned long activeSafePointSequence = 0;
    unsigned long lastDisposeSequence = 0;
    for (std::size_t i = 0; i < events.size(); ++i)
    {
      if (i > 0)
      {
        assert(events[i].sequence == events[i - 1].sequence + 1);
      }
      const NullScenePlatformController::EventKind kind = events[i].kind;
      if (kind == NullScenePlatformController::EVENT_FLUSH_BEGIN ||
          kind == NullScenePlatformController::EVENT_DRAIN_BEGIN)
      {
        if (safePointDepth == 0)
        {
          activeSafePointSequence = events[i].sequence;
          lastDisposeSequence = 0;
        }
        ++safePointDepth;
      }
      else if (kind == NullScenePlatformController::EVENT_FLUSH_END ||
               kind == NullScenePlatformController::EVENT_DRAIN_END)
      {
        assert(safePointDepth > 0);
        if (lastDisposeSequence != 0)
        {
          assert(activeSafePointSequence < lastDisposeSequence);
          assert(lastDisposeSequence < events[i].sequence);
        }
        --safePointDepth;
        if (safePointDepth == 0)
        {
          activeSafePointSequence = 0;
          lastDisposeSequence = 0;
        }
      }
      else if (kind == NullScenePlatformController::EVENT_CONTROL_DISPOSED)
      {
        assert(safePointDepth > 0);
        assert(events[i].sequence > activeSafePointSequence);
        lastDisposeSequence = events[i].sequence;
      }
    }
    assert(safePointDepth == 0);
  }
} // namespace

void testNullPlatformContract_A1_contextDestructorRunsTeardownSequence()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  visible.set(false);
  requestChildPump(scene);

  assert(platform.ledger().empty());
  assert(platform.teardownCounters().backPointerCleared == 1);
  assert(platform.teardownCounters().rowRemoved == 1);
  assert(platform.teardownCounters().handedToPool == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 1);
  g_toggleVisible = 0;
}

void testNullPlatformContract_A2_retainedDetachRunsNoTeardown()
{
  loka::core::MutableState<bool> condition(true);
  g_retainedCondition = &condition;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<RetainedButtonBoundaryNode>()));
  mountAndAttach(scene, platform);

  condition.set(false);
  requestChildPump(scene);

  assert(platform.ledger().size() == 1);
  assert(!platform.ledger()[0].visible);
  assert(platform.teardownCounters().backPointerCleared == 0);
  assert(platform.teardownCounters().rowRemoved == 0);
  assert(platform.teardownCounters().handedToPool == 0);
  g_retainedCondition = 0;
}

void testNullPlatformContract_A3_intakeConsistencyFailureLeaksWithoutPooling()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 1);
  NullScenePlatformController::FakeControlHandle *handle = platform.ledger()[0].handle;

  platform.preserveNextRetiredOwnerForTesting();
  visible.set(false);
  requestChildPump(scene);

  assert(platform.intakeCheckFailCount() == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0);
  assert(!handle->disposed);
  assert(handle->leakedDeliberately);
  assert(platform.disposedCount() == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED) == 0);
  g_toggleVisible = 0;
}

void testNullPlatformContract_B1_attachShowsControl()
{
  loka::app::ButtonDefinition button("shown");
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(button.clone());
  mountAndAttach(scene, platform);

  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].visible);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN) == 1);
}

void testNullPlatformContract_B2_retainedDetachHidesAndKeepsRow()
{
  loka::core::MutableState<bool> condition(true);
  g_retainedCondition = &condition;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<RetainedButtonBoundaryNode>()));
  mountAndAttach(scene, platform);
  const int handleId = platform.ledger()[0].handle->id;

  condition.set(false);
  requestChildPump(scene);

  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].handle->id == handleId);
  assert(!platform.ledger()[0].visible);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN) == 1);
  g_retainedCondition = 0;
}

void testNullPlatformContract_B3_reattachKeepsHandleIdentity()
{
  loka::core::MutableState<bool> condition(true);
  g_retainedCondition = &condition;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<RetainedButtonBoundaryNode>()));
  mountAndAttach(scene, platform);
  const int handleId = platform.ledger()[0].handle->id;

  condition.set(false);
  requestChildPump(scene);
  condition.set(true);
  requestChildPump(scene);

  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].handle->id == handleId);
  assert(platform.ledger()[0].visible);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN) == 2);
  g_retainedCondition = 0;
}

void testNullPlatformContract_B4_retireRemovesLedgerRow()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 1);

  visible.set(false);
  requestChildPump(scene);

  assert(platform.ledger().empty());
  assert(platform.teardownCounters().rowRemoved == 1);
  g_toggleVisible = 0;
}

void testNullPlatformContract_B5_hiddenAncestorSwapIsSilent()
{
  loka::core::MutableState<bool> ancestorVisible(true);
  loka::core::MutableState<bool> innerCondition(true);
  g_hiddenAncestorVisible = &ancestorVisible;
  g_hiddenInnerCondition = &innerCondition;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<HiddenSwapBoundaryNode>()));
  mountAndAttach(scene, platform);

  ancestorVisible.set(false);
  requestChildPump(scene);
  const unsigned long shownBefore = platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN);
  const unsigned long hiddenBefore = platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN);
  const unsigned long createdBefore = platform.eventCount(NullScenePlatformController::EVENT_CONTROL_CREATED);
  const unsigned long disposedBefore = platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED);

  innerCondition.set(false);
  requestChildPump(scene);

  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN) == shownBefore);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN) == hiddenBefore);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_CREATED) == createdBefore);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED) == disposedBefore);
  g_hiddenAncestorVisible = 0;
  g_hiddenInnerCondition = 0;
}

void testNullPlatformContract_C2_hintControlsFlushPolicy()
{
  {
    loka::core::MutableState<bool> visible(true);
    g_toggleVisible = &visible;
    g_toggleHint = loka::app::scene::NATIVE_HINT_EAGER_RELEASE;
    NullScenePlatformController platform;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
    mountAndAttach(scene, platform);
    visible.set(false);
    requestChildPump(scene);
    assert(platform.disposedCount() == 1);
    assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0);
  }
  {
    loka::core::MutableState<bool> visible(true);
    g_toggleVisible = &visible;
    g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
    NullScenePlatformController platform;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
    mountAndAttach(scene, platform);
    visible.set(false);
    requestChildPump(scene);
    assert(platform.disposedCount() == 0);
    assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 1);
  }
  {
    loka::core::MutableState<bool> visible(true);
    g_toggleVisible = &visible;
    g_toggleHint = loka::app::scene::NATIVE_HINT_DESIRE_STAY;
    NullScenePlatformController platform;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
    mountAndAttach(scene, platform);
    visible.set(false);
    requestChildPump(scene);
    assert(platform.disposedCount() == 0);
    assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 1);
  }
  g_toggleVisible = 0;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
}

void testNullPlatformContract_C3_hintChangesReachNextObservation()
{
  // Declare-time hint arrives with the attach-time read; a runtime change
  // is honored by the time the native side decides. Discriminating check:
  // the retire flush runs BEFORE any teardown drain, so a stale
  // DESIRE_STAY would pool the handle (depth 1, disposed 0) while the
  // fresh EAGER_RELEASE must dispose it (depth 0, disposed 1).
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DESIRE_STAY;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].hint == loka::app::scene::NATIVE_HINT_DESIRE_STAY &&
         "the attach-time read carries the declare-time hint");

  g_toggleHint = loka::app::scene::NATIVE_HINT_EAGER_RELEASE;
  requestChildPump(scene);

  visible.set(false);
  requestChildPump(scene);
  assert(platform.disposedCount() >= 1 &&
         "the retire flush honors the runtime hint change: EAGER_RELEASE disposes");
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0 &&
         "nothing pools under the fresh hint");

  scene.unmount();
  g_toggleVisible = 0;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
}

void testNullPlatformContract_D1_exactMatchBucketsStaySeparated()
{
  loka::core::MutableState<int> mode(1);
  g_recipeMode = &mode;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<RecipeBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 1);
  const int buttonId = platform.ledger()[0].handle->id;

  mode.set(2);
  requestChildPump(scene);
  assert(platform.ledger().size() == 1);
  const int editTextId = platform.ledger()[0].handle->id;
  assert(editTextId != buttonId);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT).missCount == 1);

  mode.set(1);
  requestChildPump(scene);
  assert(platform.ledger()[0].handle->id == buttonId);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).hitCount == 1);

  mode.set(2);
  requestChildPump(scene);
  assert(platform.ledger()[0].handle->id == editTextId);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT).hitCount == 1);
  g_recipeMode = 0;
}

void testNullPlatformContract_D2_churnProducesPoolHits()
{
  const int churnCount = 6;
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  for (int i = 0; i < churnCount; ++i)
  {
    visible.set(false);
    requestChildPump(scene);
    visible.set(true);
    requestChildPump(scene);
  }

  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).hitCount >=
         static_cast<unsigned long>(churnCount - 1));
  g_toggleVisible = 0;
}

void testNullPlatformContract_D3_depthCapRefusalCountsEvict()
{
  loka::core::MutableState<bool> visible(true);
  g_multipleVisible = &visible;
  NullScenePlatformController platform(1);
  loka::app::scene::Scene scene((loka::app::scene::Boundary<MultipleButtonBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 2);

  visible.set(false);
  requestChildPump(scene);

  NullScenePlatformController::BucketStats stats =
      platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  assert(stats.depth == 1);
  assert(stats.evictCount == 1);
  assert(platform.disposedCount() == 1);
  g_multipleVisible = 0;
}

void testNullPlatformContract_D4_controllerDrainPrecedesWindowDispose()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
  NullPlatformContext context;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  {
    WindowProps props;
    NullWindow window(&context, props, &platform);
    mountAndAttach(scene, platform);
    visible.set(false);
    requestChildPump(scene);
    assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 1);
  }

  unsigned long drainSequence = 0;
  unsigned long windowSequence = 0;
  const std::vector<NullScenePlatformController::EventRecord> &events = platform.eventLog();
  for (std::size_t i = 0; i < events.size(); ++i)
  {
    if (events[i].kind == NullScenePlatformController::EVENT_DRAIN_END)
    {
      drainSequence = events[i].sequence;
    }
    else if (events[i].kind == NullScenePlatformController::EVENT_WINDOW_DISPOSED)
    {
      windowSequence = events[i].sequence;
    }
  }
  assert(drainSequence != 0);
  assert(windowSequence > drainSequence);
  assertDisposalsAreInsideSafePoints(platform);
  scene.unmount();
  g_toggleVisible = 0;
}

void testNullPlatformContract_E1_reclaimOnlyFlushIsSilent()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  visible.set(false);
  requestChildPump(scene);
  const std::size_t eventCountBeforeReclaim = platform.eventLog().size();
  assert(!scene.flushInvalidation());
  assert(platform.eventLog().size() == eventCountBeforeReclaim);
  g_toggleVisible = 0;
}

void testNullPlatformContract_E2_disposeOccursOnlyAtSafePoints()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_EAGER_RELEASE;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);
  visible.set(false);
  requestChildPump(scene);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED) == 1);
  assertDisposalsAreInsideSafePoints(platform);
  g_toggleVisible = 0;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
}

void testNullPlatformContract_E3_parkedBranchRetiresAtTheDoorNotAtReclaim()
{
  // A Conditional parks a branch, then its whole subtree leaves the
  // composition. However the composition diffs slice it (swap, replacement,
  // removal), every native pair must be handed over at a retire door —
  // synchronously with some pump — never from the deferred reclaim drain.
  // The invariant: once the pumps settle, a reclaim-only flush is silent
  // and leaves no half-processed handles behind.
  loka::core::MutableState<bool> visible(true);
  loka::core::MutableState<bool> inner(true);
  g_parkedSubtreeVisible = &visible;
  g_parkedInnerCondition = &inner;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ParkedBranchRetireBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 1);

  // The swap can retire the old conditional through the composition diff
  // (definition identity is per-compose); whatever got queued, the follow-up
  // reclaim-only drain must destroy it silently — its contexts were already
  // handed over at the door.
  inner.set(false);
  std::size_t eventsBeforeDrain = platform.eventLog().size();
  assert(!scene.flushInvalidation());
  assert(platform.eventLog().size() == eventsBeforeDrain &&
         "reclamation is silent — no fact reaches a context from the drain");
  assert(platform.retiredCount() == 0);

  visible.set(false);
  requestChildPump(scene);
  assert(platform.ledger().empty() &&
         "every native pair is handed over at a retire door, not parked past it");
  assert(platform.retiredCount() == 0);

  eventsBeforeDrain = platform.eventLog().size();
  assert(!scene.flushInvalidation());
  assert(platform.eventLog().size() == eventsBeforeDrain &&
         "the final drain is silent too");
  assert(platform.retiredCount() == 0);
  scene.unmount();
  assert(platform.createdCount() == platform.disposedCount() + platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth &&
         "teardown closes every pair: disposed or pooled, nothing lost");
  g_parkedSubtreeVisible = 0;
  g_parkedInnerCondition = 0;
}

void testNullPlatformContract_H1_conditionalSeatSurvivesUnrelatedRecompose()
{
  ParkedFactRecord probeRecord;
  loka::core::MutableState<bool> condition(false);
  loka::core::MutableState<int> unrelated(0);
  loka::core::MutableState<loka::core::String> draft(loka::core::String::Literal("parked draft"));
  g_seatCondition = &condition;
  g_seatUnrelatedState = &unrelated;
  g_seatDraft = &draft;
  g_seatProbeRecord = &probeRecord;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ConditionalSeatHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(probeRecord);
  const int constructionsBefore = probeRecord.constructionCount;
  const std::size_t transitionsBefore = probeRecord.transitions.size();
  const NativeContextCallCounts callsBefore(platform);

  unrelated.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  const NativeContextCallCounts callsAfter(platform);
  const bool parkedProbeSurvived =
      probeRecord.constructionCount == constructionsBefore &&
      !recordedTransitionTo(probeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            transitionsBefore);
  const bool nativeContextCallsStayedEqual = callsAfter == callsBefore;

  assert(parkedProbeSurvived && nativeContextCallsStayedEqual &&
         "the retained Conditional seat preserves its parked branch and native pairs");

  scene.unmount();
  g_seatCondition = 0;
  g_seatUnrelatedState = 0;
  g_seatDraft = 0;
  g_seatProbeRecord = 0;
}

void testNullPlatformContract_H2_parkedDraftBranchSurvivesUnrelatedRecompose()
{
  ParkedFactRecord probeRecord;
  loka::core::MutableState<bool> condition(false);
  loka::core::MutableState<int> unrelated(0);
  loka::core::MutableState<loka::core::String> draft(loka::core::String::Literal("unfinished draft"));
  g_seatCondition = &condition;
  g_seatUnrelatedState = &unrelated;
  g_seatDraft = &draft;
  g_seatProbeRecord = &probeRecord;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ConditionalSeatHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(probeRecord);
  const int constructionsBefore = probeRecord.constructionCount;
  const std::size_t transitionsBefore = probeRecord.transitions.size();
  const NativeContextCallCounts callsBefore(platform);

  unrelated.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  const NativeContextCallCounts callsAfter(platform);
  const bool parkedDraftSurvived =
      probeRecord.constructionCount == constructionsBefore &&
      !recordedTransitionTo(probeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            transitionsBefore) &&
      callsAfter == callsBefore &&
      draft.get().equals(loka::core::String::Literal("unfinished draft"));

  assert(parkedDraftSurvived &&
         "the retained Conditional seat preserves parked branch state");

  scene.unmount();
  g_seatCondition = 0;
  g_seatUnrelatedState = 0;
  g_seatDraft = 0;
  g_seatProbeRecord = 0;
}

void testConditionalSeatRepointsBranchDefinitionsAfterUnrelatedRecompose()
{
  ParkedFactRecord oldBranchRecord;
  ParkedFactRecord currentBranchRecord;
  loka::core::MutableState<bool> condition(true);
  loka::core::MutableState<int> unrelated(0);
  loka::core::MutableState<loka::core::String> draft;
  g_seatCondition = &condition;
  g_seatUnrelatedState = &unrelated;
  g_seatDraft = &draft;
  g_seatOldBranchRecord = &oldBranchRecord;
  g_seatCurrentBranchRecord = &currentBranchRecord;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ConditionalSeatHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  assert(oldBranchRecord.constructionCount == 0);
  assert(currentBranchRecord.constructionCount == 0);
  const NativeContextCallCounts callsBefore(platform);

  unrelated.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(NativeContextCallCounts(platform) == callsBefore &&
         "an unrelated recompose retains the Conditional seat and its native pair");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(oldBranchRecord.constructionCount == 0 &&
         "the retained seat no longer reads the previous arena's branch definition");
  assert(currentBranchRecord.constructionCount == 1 &&
         "the retained seat creates the branch from the current arena's definition");

  scene.unmount();
  g_seatCondition = 0;
  g_seatUnrelatedState = 0;
  g_seatDraft = 0;
  g_seatOldBranchRecord = 0;
  g_seatCurrentBranchRecord = 0;
}

void testNullPlatformContract_H3_conditionFlipIsReflectedAtNextScheduledApply()
{
  ParkedFactRecord trueRecord;
  ParkedFactRecord falseRecord;
  loka::core::MutableState<bool> condition(true);
  g_deferredFlipCondition = &condition;
  g_deferredTrueRecord = &trueRecord;
  g_deferredFalseRecord = &falseRecord;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<DeferredFlipBoundaryNode>()));
  mountAndAttach(scene, platform);

  assert(trueRecord.constructionCount == 1);
  assert(falseRecord.constructionCount == 0);
  const int trueConstructionsBefore = trueRecord.constructionCount;
  const int falseConstructionsBefore = falseRecord.constructionCount;
  const std::size_t trueTransitionsBefore = trueRecord.transitions.size();
  const std::size_t falseTransitionsBefore = falseRecord.transitions.size();
  const NativeContextCallCounts callsBefore(platform);
  const std::size_t eventsBefore = platform.eventLog().size();
  const std::size_t ledgerRowsBefore = platform.ledger().size();
  const NullScenePlatformController::LedgerRow *buttonBefore =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const NullScenePlatformController::LedgerRow *editTextBefore =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  const bool buttonVisibleBefore = buttonBefore && buttonBefore->visible;
  const bool editTextVisibleBefore = editTextBefore && editTextBefore->visible;

  condition.set(false);
  assert(scene.hasPendingInvalidation());

  const NativeContextCallCounts callsAfterWrite(platform);
  const bool probeFactsStayedUnchanged =
      trueRecord.constructionCount == trueConstructionsBefore &&
      falseRecord.constructionCount == falseConstructionsBefore &&
      trueRecord.transitions.size() == trueTransitionsBefore &&
      falseRecord.transitions.size() == falseTransitionsBefore;
  const bool nativeContextCallsStayedEqual = callsAfterWrite == callsBefore;
  const bool nativeEventLogStayedEqual = platform.eventLog().size() == eventsBefore;
  const NullScenePlatformController::LedgerRow *buttonAfterWrite =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const NullScenePlatformController::LedgerRow *editTextAfterWrite =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  const bool nativeLedgerStayedEqual =
      platform.ledger().size() == ledgerRowsBefore &&
      (buttonAfterWrite != 0) == (buttonBefore != 0) &&
      (editTextAfterWrite != 0) == (editTextBefore != 0) &&
      (!buttonAfterWrite || buttonAfterWrite->visible == buttonVisibleBefore) &&
      (!editTextAfterWrite || editTextAfterWrite->visible == editTextVisibleBefore);

  assert(probeFactsStayedUnchanged &&
         nativeContextCallsStayedEqual &&
         nativeEventLogStayedEqual &&
         nativeLedgerStayedEqual &&
         "a condition flip remains unobservable until the next scheduled apply");

  assert(scene.flushInvalidation());
  const NullScenePlatformController::LedgerRow *button =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const NullScenePlatformController::LedgerRow *editText =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(button && !button->visible);
  assert(editText && editText->visible);

  scene.unmount();
  g_deferredFlipCondition = 0;
  g_deferredTrueRecord = 0;
  g_deferredFalseRecord = 0;
}

void testNullPlatformContract_H4_retiringBoundaryReportsEveryParkedBranchRetiredInSameTick()
{
  ParkedFactRecord firstRecord;
  ParkedFactRecord secondRecord;
  loka::core::MutableState<bool> visible(true);
  loka::core::MutableState<bool> firstCondition(false);
  loka::core::MutableState<bool> secondCondition(false);
  g_enumeratedSubtreeVisible = &visible;
  g_enumeratedFirstCondition = &firstCondition;
  g_enumeratedSecondCondition = &secondCondition;
  g_enumeratedFirstRecord = &firstRecord;
  g_enumeratedSecondRecord = &secondRecord;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<EnumeratedBranchesHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  assert(firstRecord.constructionCount == 1);
  assert(secondRecord.constructionCount == 1);
  secondCondition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(secondRecord);

  firstCondition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(firstRecord);
  assert(firstRecord.transitions.size() == secondRecord.transitions.size());

  visible.set(false);

  // The state write's immediate-flush floor is one tick. Both parked facts
  // must already be terminal here; no follow-up pump may be needed.
  assert(firstRecord.transitions.size() == secondRecord.transitions.size());
  assertParkedRetirementTransitionTable(firstRecord);
  assertParkedRetirementTransitionTable(secondRecord);

  scene.unmount();
  g_enumeratedSubtreeVisible = 0;
  g_enumeratedFirstCondition = 0;
  g_enumeratedSecondCondition = 0;
  g_enumeratedFirstRecord = 0;
  g_enumeratedSecondRecord = 0;
}

void testNullPlatformContract_H5_taggedSeatAmongSiblingsSurvivesUnrelatedRecompose()
{
  ParkedFactRecord probeRecord;
  loka::core::MutableState<bool> condition(false);
  loka::core::MutableState<int> unrelated(0);
  loka::core::MutableState<loka::core::String> draft(loka::core::String::Literal("tagged parked draft"));
  g_seatCondition = &condition;
  g_seatUnrelatedState = &unrelated;
  g_seatDraft = &draft;
  g_seatProbeRecord = &probeRecord;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<TaggedConditionalSeatHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(probeRecord);
  const int constructionsBefore = probeRecord.constructionCount;
  const std::size_t transitionsBefore = probeRecord.transitions.size();
  const NativeContextCallCounts callsBefore(platform);

  unrelated.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  const bool taggedSeatSurvived =
      probeRecord.constructionCount == constructionsBefore &&
      !recordedTransitionTo(probeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            transitionsBefore) &&
      NativeContextCallCounts(platform) == callsBefore;
  assert(taggedSeatSurvived &&
         "the tagged Conditional seat preserves its parked branch and native pairs");

  scene.unmount();
  g_seatCondition = 0;
  g_seatUnrelatedState = 0;
  g_seatDraft = 0;
  g_seatProbeRecord = 0;
}

void testNullPlatformContract_H6_activeBranchContentIsFreshAfterRecompose()
{
  ParkedFactRecord activeRecord;
  ParkedFactRecord parkedRecord;
  loka::core::MutableState<bool> condition(false);
  loka::core::MutableState<int> revision(0);
  ConditionalContentInputs inputs(&condition, &revision, 0, &activeRecord, &parkedRecord);
  g_contentInputs = &inputs;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<ConditionalContentHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(parkedRecord);
  const int constructionsBefore = parkedRecord.constructionCount;
  const std::size_t transitionsBefore = parkedRecord.transitions.size();

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  const NullScenePlatformController::LedgerRow *buttonAfter =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const bool seatRetained =
      parkedRecord.constructionCount == constructionsBefore &&
      !recordedTransitionTo(parkedRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            transitionsBefore);
  const bool contentFresh =
      buttonAfter && buttonAfter->visible && buttonAfter->handle &&
      buttonAfter->handle->owner &&
      buttonAfter->handle->owner->lifetimeHint() ==
          loka::app::scene::NATIVE_HINT_DESIRE_STAY;
  assert(seatRetained &&
         "the Conditional seat remains present across active-branch content recompose");

  // Expected RED: T4 promotes this pin when the active branch exposes the
  // recomposed constant content after the pump settles.
  assertExpectedRedForT4(seatRetained && contentFresh);

  scene.unmount();
  g_contentInputs = 0;
}

void testNullPlatformContract_H7_reenteredBranchContentIsFreshAfterRecompose()
{
  ParkedFactRecord activeRecord;
  ParkedFactRecord parkedRecord;
  loka::core::MutableState<bool> condition(false);
  loka::core::MutableState<int> revision(0);
  ConditionalContentInputs inputs(&condition, 0, &revision, &activeRecord, &parkedRecord);
  g_contentInputs = &inputs;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<ConditionalContentHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  const NullScenePlatformController::LedgerRow *parkedBefore =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(parkedBefore && parkedBefore->visible);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(parkedRecord);
  const int activeConstructionsBefore = activeRecord.constructionCount;
  const std::size_t activeTransitionsBefore = activeRecord.transitions.size();

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  const bool seatRetainedThroughRecompose =
      activeRecord.constructionCount == activeConstructionsBefore &&
      !recordedTransitionTo(activeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            activeTransitionsBefore);
  assert(seatRetainedThroughRecompose &&
         "the Conditional seat remains present across parked-branch content recompose");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  const NullScenePlatformController::LedgerRow *parkedAfter =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  const bool seatRetained =
      seatRetainedThroughRecompose &&
      activeRecord.constructionCount == activeConstructionsBefore &&
      !recordedTransitionTo(activeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            activeTransitionsBefore);
  const bool contentFresh =
      parkedAfter && parkedAfter->visible && parkedAfter->handle &&
      parkedAfter->handle->owner &&
      parkedAfter->handle->owner->lifetimeHint() ==
          loka::app::scene::NATIVE_HINT_DESIRE_STAY;
  assert(seatRetained &&
         "the Conditional seat remains present through branch re-entry");

  // Expected RED: T3 promotes this pin when the re-entered branch exposes the
  // recomposed constant content after re-entry.
  assertExpectedRedForT3(seatRetained && contentFresh);

  scene.unmount();
  g_contentInputs = 0;
}

void testNullPlatformContract_H8_taggedSeatBuildsBranchFromLiveDefinition()
{
  ParkedFactRecord liveSourceRecord;
  ParkedFactRecord expiredSourceRecord;
  loka::core::MutableState<bool> firstCondition(true);
  loka::core::MutableState<bool> secondCondition(true);
  loka::core::MutableState<bool> currentCondition(true);
  TaggedPropsApplyInputs inputs(&firstCondition);
  loka::app::ButtonDefinition active("tagged-props-apply-active");
  DefinitionSourceProbeDefinition parked(&liveSourceRecord, &expiredSourceRecord);
  TaggedPropsApplyConditionalDefinition conditional(
      loka::app::scene::ConditionalProps(&firstCondition, &active, &parked),
      &inputs);
  conditional.setNodeTag(201);
  loka::app::FragmentDefinition *root = new loka::app::FragmentDefinition();
  (*root) << conditional;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(static_cast<loka::app::scene::NodeDefinitionBase *>(root));
  mountAndAttach(scene, platform);

  assert(liveSourceRecord.constructionCount == 0);
  assert(expiredSourceRecord.constructionCount == 0);

  // Each changed condition pointer drives RootBoundaryWrapper's tagged-child
  // props apply and turns over the snapshot generation without swapping seats.
  inputs.condition = &secondCondition;
  requestChildPump(scene);
  inputs.condition = &currentCondition;
  requestChildPump(scene);

  currentCondition.set(false);
  if (scene.hasPendingInvalidation())
  {
    assert(scene.flushInvalidation());
  }
  assert(liveSourceRecord.constructionCount == 1 &&
         expiredSourceRecord.constructionCount == 0 &&
         "the tagged Conditional builds its branch from a live-arena definition");

  scene.unmount();
}

void testNullPlatformContract_H9_retainedSeatUsesReplacementCondition()
{
  ParkedFactRecord activeRecord;
  ParkedFactRecord parkedRecord;
  loka::core::MutableState<bool> previousCondition(false);
  loka::core::MutableState<bool> replacementCondition(false);
  TaggedPropsApplyInputs inputs(&previousCondition);
  ParkedFactDefinition activeProbe((ParkedFactProps(&activeRecord)));
  loka::app::ButtonDefinition activeControl("replacement-condition-active");
  loka::app::FragmentDefinition activeBranch;
  activeBranch << activeProbe << activeControl;
  ParkedFactDefinition parkedProbe((ParkedFactProps(&parkedRecord)));
  loka::app::EditTextDefinition parkedControl;
  loka::app::FragmentDefinition parkedBranch;
  parkedBranch << parkedProbe << parkedControl;
  TaggedPropsApplyConditionalDefinition conditional(
      loka::app::scene::ConditionalProps(&previousCondition, &activeBranch, &parkedBranch),
      &inputs);
  conditional.setNodeTag(301);
  loka::app::FragmentDefinition *root = new loka::app::FragmentDefinition();
  (*root) << conditional;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(static_cast<loka::app::scene::NodeDefinitionBase *>(root));
  mountAndAttach(scene, platform);

  assert(activeRecord.constructionCount == 0);
  assert(parkedRecord.constructionCount == 1);
  const int parkedConstructionsBefore = parkedRecord.constructionCount;
  const std::size_t parkedTransitionsBefore = parkedRecord.transitions.size();
  const NativeContextCallCounts callsBeforeRebind(platform);

  inputs.condition = &replacementCondition;
  requestChildPump(scene);

  assert(parkedRecord.constructionCount == parkedConstructionsBefore &&
         parkedRecord.transitions.size() == parkedTransitionsBefore &&
         NativeContextCallCounts(platform) == callsBeforeRebind &&
         "re-binding the retained seat preserves its active branch and native pairs");

  previousCondition.set(true);
  assert(!scene.hasPendingInvalidation());
  assert(activeRecord.constructionCount == 0 &&
         parkedRecord.transitions.size() == parkedTransitionsBefore &&
         NativeContextCallCounts(platform) == callsBeforeRebind &&
         "the previous condition no longer drives the retained seat");

  replacementCondition.set(true);
  if (scene.hasPendingInvalidation())
  {
    assert(scene.flushInvalidation());
  }
  const NullScenePlatformController::LedgerRow *button =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const NullScenePlatformController::LedgerRow *editText =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(activeRecord.constructionCount == 1);
  assertParkedTransitionTable(parkedRecord);
  assert(button && button->visible);
  assert(editText && !editText->visible);

  const int activeConstructionsBefore = activeRecord.constructionCount;
  const std::size_t activeTransitionsBefore = activeRecord.transitions.size();
  const NativeContextCallCounts callsBeforeSecondFlip(platform);
  replacementCondition.set(false);
  if (scene.hasPendingInvalidation())
  {
    assert(activeRecord.constructionCount == activeConstructionsBefore &&
           activeRecord.transitions.size() == activeTransitionsBefore &&
           NativeContextCallCounts(platform) == callsBeforeSecondFlip);
    assert(scene.flushInvalidation());
  }
  assert(button && !button->visible);
  assert(editText && editText->visible);

  scene.unmount();
}

void testNullPlatformContract_F1_retiredQueueIsEmptyAfterFlush()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);
  visible.set(false);
  requestChildPump(scene);
  assert(platform.retiredCount() == 0);
  g_toggleVisible = 0;
}

void testNullPlatformContract_F2_createdHandlesAreDisposedAtTeardown()
{
  loka::app::FragmentDefinition controls;
  controls << loka::app::Button("button") << loka::app::EditText();
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(controls.clone());
  mountAndAttach(scene, platform);
  assert(platform.createdCount() == 2);

  scene.unmount();

  assert(platform.ledger().empty());
  assert(platform.retiredCount() == 0);
  assert(platform.createdCount() == platform.disposedCount());
  const std::vector<NullScenePlatformController::FakeControlHandle *> &handles = platform.allHandles();
  for (std::size_t i = 0; i < handles.size(); ++i)
  {
    assert(handles[i]->disposed);
  }
}

void testNullPlatformContract_G4_retireBeforeContextAttachIsSilent()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  platform.skipNextProjectionForTesting();
  mountAndAttach(scene, platform);
  assert(platform.ledger().empty());

  visible.set(false);
  requestChildPump(scene);

  assert(platform.ledger().empty());
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_CREATED) == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN) == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN) == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED) == 0);
  assert(platform.teardownCounters().rowRemoved == 0);
  g_toggleVisible = 0;
}

void testNullWindowScenePathMountsAndTearsDownBeforeControllerDelete()
{
  // Window path with a borrowed controller: the props-attached scene must
  // reach the ledger through NullWindow itself, and window destruction must
  // unmount it (drain before the window-dispose record, all pairs closed).
  {
    NullPlatformContext platformContext;
    NullScenePlatformController platform;
    loka::app::ButtonDefinition button("window-path");
    WindowProps props;
    props.scene(new loka::app::scene::Scene(button.clone()));
    NullWindow *window = new NullWindow(&platformContext, props, &platform);
    loka::app::scene::Scene *scene = window->scene();
    assert(scene);
    scene->updateAttached(true);
    assert(platform.ledger().size() == 1);
    assert(platform.ledger()[0].visible);

    delete window;

    assert(platform.ledger().empty());
    assert(platform.retiredCount() == 0);
    assert(platform.createdCount() == platform.disposedCount());
    assert(platform.eventCount(NullScenePlatformController::EVENT_WINDOW_DISPOSED) == 1);
    assertDisposalsAreInsideSafePoints(platform);
  }
  // Window path with an owned controller: scene teardown must complete
  // before the controller is deleted (the sanitizer guards this arm — the
  // scene manager destroys the scene after ~NullWindow's body has run).
  {
    NullPlatformContext platformContext;
    loka::app::ButtonDefinition button("window-path-owned");
    WindowProps props;
    props.scene(new loka::app::scene::Scene(button.clone()));
    Window *window = platformContext.createWindow(props);
    assert(window->scene());
    window->scene()->updateAttached(true);
    delete window;
  }
}

void testNullPlatformContract_G6_materializedChildIsVisibleInSameRun()
{
  loka::core::MutableState<bool> visible(false);
  g_toggleVisible = &visible;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().empty());

  visible.set(true);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(scene.flushInvalidation());

  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].visible);
  g_toggleVisible = 0;
}
