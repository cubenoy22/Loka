#include "NullPlatformContractTests.hpp"

#include <cassert>

#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
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
  loka::core::MutableState<bool> *g_toggleVisible = 0;
  loka::app::scene::NativeLifetimeHint g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;

  class ToggleControlBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ToggleControlBoundaryNode> ToggleControlBoundaryProps;

  class ToggleControlBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<ToggleControlBoundaryNode, ToggleControlBoundaryProps>
  {
  public:
    explicit ToggleControlBoundaryNode(const ToggleControlBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<ToggleControlBoundaryNode, ToggleControlBoundaryProps>(props)
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
      : public SceneTestSupport::RecomposingBoundaryNode<RecipeBoundaryNode, RecipeBoundaryProps>
  {
  public:
    explicit RecipeBoundaryNode(const RecipeBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<RecipeBoundaryNode, RecipeBoundaryProps>(props)
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
      : public SceneTestSupport::RecomposingBoundaryNode<MultipleButtonBoundaryNode, MultipleButtonBoundaryProps>
  {
  public:
    explicit MultipleButtonBoundaryNode(const MultipleButtonBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<MultipleButtonBoundaryNode, MultipleButtonBoundaryProps>(props)
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
      : public SceneTestSupport::RecomposingBoundaryNode<ParkedBranchRetireBoundaryNode,
                                                        ParkedBranchRetireBoundaryProps>
  {
  public:
    explicit ParkedBranchRetireBoundaryNode(const ParkedBranchRetireBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<ParkedBranchRetireBoundaryNode, ParkedBranchRetireBoundaryProps>(
              props)
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
          node(0),
          transitions()
    {
    }

    int constructionCount;
    int attachReads;
    loka::app::scene::Node *node;
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
        this->props.record->node = this;
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

  class ShowDefinitionSourceProbeDefinition
      : public loka::app::scene::NodeDefinition<ParkedFactProps,
                                                ParkedFactNode>
  {
  public:
    typedef loka::app::scene::NodeDefinition<ParkedFactProps,
                                              ParkedFactNode>
        BaseType;

    ShowDefinitionSourceProbeDefinition(ParkedFactRecord *liveRecord,
                                        ParkedFactRecord *expiredRecord)
        : BaseType(ParkedFactProps(liveRecord)),
          liveRecord_(liveRecord),
          expiredRecord_(expiredRecord)
    {
    }

    virtual ~ShowDefinitionSourceProbeDefinition()
    {
      this->props.record = this->expiredRecord_;
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      ShowDefinitionSourceProbeDefinition *copy =
          new ShowDefinitionSourceProbeDefinition(this->liveRecord_, this->expiredRecord_);
      if (copy)
      {
        copy->copyTestIdPolicyFrom(*this);
      }
      return copy;
    }

    virtual bool applyPropsToNode(loka::app::scene::Node *node) const
    {
      if (this->props.record)
      {
        ++this->props.record->attachReads;
        this->props.record->node = node;
      }
      return BaseType::applyPropsToNode(node);
    }

  private:
    ParkedFactRecord *liveRecord_;
    ParkedFactRecord *expiredRecord_;
  };

  class RetainedApplyFailureParkedFactDefinition
      : public loka::app::scene::NodeDefinition<ParkedFactProps,
                                                ParkedFactNode>
  {
  public:
    typedef loka::app::scene::NodeDefinition<ParkedFactProps,
                                              ParkedFactNode>
        BaseType;

    explicit RetainedApplyFailureParkedFactDefinition(ParkedFactRecord *record)
        : BaseType(ParkedFactProps(record))
    {
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      RetainedApplyFailureParkedFactDefinition *copy =
          new RetainedApplyFailureParkedFactDefinition(this->props.record);
      if (copy)
      {
        copy->copyTestIdPolicyFrom(*this);
      }
      return copy;
    }

    virtual bool applyPropsToNode(loka::app::scene::Node *) const
    {
      return false;
    }
  };

  struct NestedSeatReentryInputs
  {
    NestedSeatReentryInputs(loka::core::MutableState<bool> *outerConditionState,
                            loka::core::MutableState<bool> *innerConditionState,
                            loka::core::MutableState<int> *revisionState,
                            ParkedFactRecord *oldRecord,
                            ParkedFactRecord *currentRecord,
                            ParkedFactRecord *expiredRecord)
        : outerCondition(outerConditionState),
          innerCondition(innerConditionState),
          revision(revisionState),
          oldSource(oldRecord),
          currentSource(currentRecord),
          expiredSource(expiredRecord)
    {
    }

    loka::core::MutableState<bool> *outerCondition;
    loka::core::MutableState<bool> *innerCondition;
    loka::core::MutableState<int> *revision;
    ParkedFactRecord *oldSource;
    ParkedFactRecord *currentSource;
    ParkedFactRecord *expiredSource;
  };

  NestedSeatReentryInputs *g_nestedSeatReentryInputs = 0;

  class NestedSeatReentryBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<NestedSeatReentryBoundaryNode>
      NestedSeatReentryBoundaryProps;
  class NestedSeatReentryBoundaryNode
      : public PropsRecomposingBoundaryNode<NestedSeatReentryBoundaryNode,
                                            NestedSeatReentryBoundaryProps>
  {
  public:
    explicit NestedSeatReentryBoundaryNode(const NestedSeatReentryBoundaryProps &props)
        : PropsRecomposingBoundaryNode<NestedSeatReentryBoundaryNode,
                                       NestedSeatReentryBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_nestedSeatReentryInputs && g_nestedSeatReentryInputs->revision)
      {
        registrar.markDirtyOnChange(g_nestedSeatReentryInputs->revision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      const bool revised =
          g_nestedSeatReentryInputs && g_nestedSeatReentryInputs->revision &&
          g_nestedSeatReentryInputs->revision->get() != 0;
      DefinitionSourceProbeDefinition nestedShown(
          revised ? g_nestedSeatReentryInputs->currentSource
                  : g_nestedSeatReentryInputs->oldSource,
          g_nestedSeatReentryInputs->expiredSource);
      loka::app::EditTextDefinition nestedHidden;
      loka::app::scene::ConditionalDefinition nested(
          (loka::app::scene::ConditionalProps(
              g_nestedSeatReentryInputs ? g_nestedSeatReentryInputs->innerCondition : 0,
              &nestedShown,
              &nestedHidden)));
      loka::app::FragmentDefinition parkedBranch;
      parkedBranch << nested;
      loka::app::ButtonDefinition activeBranch("nested-seat-active");
      loka::app::scene::ConditionalDefinition outer(
          (loka::app::scene::ConditionalProps(
              g_nestedSeatReentryInputs ? g_nestedSeatReentryInputs->outerCondition : 0,
              &activeBranch,
              &parkedBranch)));
      loka::app::FragmentDefinition root;
      root << outer;
      composition.declare(root);
    }
  };

  class NestedSeatReentryHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<NestedSeatReentryHarnessBoundaryNode>
      NestedSeatReentryHarnessBoundaryProps;
  class NestedSeatReentryHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<NestedSeatReentryHarnessBoundaryNode>
  {
  public:
    explicit NestedSeatReentryHarnessBoundaryNode(
        const NestedSeatReentryHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<NestedSeatReentryHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::scene::Boundary<NestedSeatReentryBoundaryNode>());
    }
  };

  struct ShowReentryInputs
  {
    ShowReentryInputs(loka::core::MutableState<bool> *conditionState,
                      loka::core::MutableState<int> *revisionState,
                      ParkedFactRecord *oldRecord,
                      ParkedFactRecord *currentRecord,
                      ParkedFactRecord *expiredRecord)
        : condition(conditionState),
          revision(revisionState),
          oldSource(oldRecord),
          currentSource(currentRecord),
          expiredSource(expiredRecord),
          definitionReuseBlockers()
    {
    }

    ~ShowReentryInputs()
    {
      for (size_t i = 0; i < this->definitionReuseBlockers.size(); ++i)
      {
        delete this->definitionReuseBlockers[i];
      }
    }

    void blockFreedShowDefinitionAddress()
    {
      this->definitionReuseBlockers.push_back(
          new loka::app::ShowDefinition(loka::app::Show(*this->condition)));
    }

    loka::core::MutableState<bool> *condition;
    loka::core::MutableState<int> *revision;
    ParkedFactRecord *oldSource;
    ParkedFactRecord *currentSource;
    ParkedFactRecord *expiredSource;
    std::vector<loka::app::ShowDefinition *> definitionReuseBlockers;
  };

  ShowReentryInputs *g_showReentryInputs = 0;

  class ShowReentryBoundaryNode;
  ShowReentryBoundaryNode *g_showReentryBoundaryNode = 0;
  typedef loka::app::scene::BoundaryPropsFor<ShowReentryBoundaryNode>
      ShowReentryBoundaryProps;
  class ShowReentryBoundaryNode
      : public PropsRecomposingBoundaryNode<ShowReentryBoundaryNode,
                                            ShowReentryBoundaryProps>
  {
  public:
    explicit ShowReentryBoundaryNode(const ShowReentryBoundaryProps &props)
        : PropsRecomposingBoundaryNode<ShowReentryBoundaryNode,
                                       ShowReentryBoundaryProps>(props)
    {
      g_showReentryBoundaryNode = this;
    }

    virtual ~ShowReentryBoundaryNode()
    {
      if (g_showReentryBoundaryNode == this)
      {
        g_showReentryBoundaryNode = 0;
      }
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_showReentryInputs && g_showReentryInputs->revision)
      {
        registrar.markDirtyOnChange(g_showReentryInputs->revision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      const bool current =
          g_showReentryInputs && g_showReentryInputs->revision &&
          g_showReentryInputs->revision->get() == 2;
      const bool revised =
          g_showReentryInputs && g_showReentryInputs->revision &&
          g_showReentryInputs->revision->get() != 0;
      if (revised)
      {
        g_showReentryInputs->blockFreedShowDefinitionAddress();
      }
      ShowDefinitionSourceProbeDefinition probe(
          revised ? g_showReentryInputs->currentSource : g_showReentryInputs->oldSource,
          g_showReentryInputs->expiredSource);
      loka::app::EditTextDefinition control;
      control.lifetimeHint(current ? loka::app::scene::NATIVE_HINT_DESIRE_STAY
                                   : loka::app::scene::NATIVE_HINT_DEFAULT);
      loka::app::ShowDefinition shown =
          loka::app::Show(*g_showReentryInputs->condition);
      shown << probe << control;
      loka::app::FragmentDefinition root;
      root << shown;
      composition.declare(root);
    }

  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      if (event != loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        typedef loka::app::scene::BoundaryNodeFor<ShowReentryBoundaryNode> BaseType;
        BaseType::composeWithContext(context, event);
        return;
      }
      this->recomposeLocalComposition(
          context, event, this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS);
    }
  };

  class ShowReentryHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ShowReentryHarnessBoundaryNode>
      ShowReentryHarnessBoundaryProps;
  class ShowReentryHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<ShowReentryHarnessBoundaryNode>
  {
  public:
    explicit ShowReentryHarnessBoundaryNode(const ShowReentryHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<ShowReentryHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::scene::Boundary<ShowReentryBoundaryNode>());
    }
  };

  class Depth2NestedSeatReentryBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<Depth2NestedSeatReentryBoundaryNode>
      Depth2NestedSeatReentryBoundaryProps;
  class Depth2NestedSeatReentryBoundaryNode
      : public PropsRecomposingBoundaryNode<Depth2NestedSeatReentryBoundaryNode,
                                            Depth2NestedSeatReentryBoundaryProps>
  {
  public:
    explicit Depth2NestedSeatReentryBoundaryNode(
        const Depth2NestedSeatReentryBoundaryProps &props)
        : PropsRecomposingBoundaryNode<Depth2NestedSeatReentryBoundaryNode,
                                       Depth2NestedSeatReentryBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_nestedSeatReentryInputs && g_nestedSeatReentryInputs->revision)
      {
        registrar.markDirtyOnChange(g_nestedSeatReentryInputs->revision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      const bool revised =
          g_nestedSeatReentryInputs && g_nestedSeatReentryInputs->revision &&
          g_nestedSeatReentryInputs->revision->get() != 0;
      DefinitionSourceProbeDefinition nestedShown(
          revised ? g_nestedSeatReentryInputs->currentSource
                  : g_nestedSeatReentryInputs->oldSource,
          g_nestedSeatReentryInputs->expiredSource);
      loka::app::EditTextDefinition nestedHidden;
      loka::app::scene::ConditionalDefinition nested(
          (loka::app::scene::ConditionalProps(
              g_nestedSeatReentryInputs ? g_nestedSeatReentryInputs->innerCondition : 0,
              &nestedShown,
              &nestedHidden)));
      loka::app::FragmentDefinition intermediate;
      intermediate << nested;
      loka::app::FragmentDefinition parkedBranch;
      parkedBranch << intermediate;
      loka::app::ButtonDefinition activeBranch("depth-2-nested-seat-active");
      loka::app::scene::ConditionalDefinition outer(
          (loka::app::scene::ConditionalProps(
              g_nestedSeatReentryInputs ? g_nestedSeatReentryInputs->outerCondition : 0,
              &activeBranch,
              &parkedBranch)));
      loka::app::FragmentDefinition root;
      root << outer;
      composition.declare(root);
    }
  };

  class Depth2NestedSeatReentryHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<Depth2NestedSeatReentryHarnessBoundaryNode>
      Depth2NestedSeatReentryHarnessBoundaryProps;
  class Depth2NestedSeatReentryHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<Depth2NestedSeatReentryHarnessBoundaryNode>
  {
  public:
    explicit Depth2NestedSeatReentryHarnessBoundaryNode(
        const Depth2NestedSeatReentryHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<Depth2NestedSeatReentryHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::scene::Boundary<Depth2NestedSeatReentryBoundaryNode>());
    }
  };

  class FullRebuildLedgerDefinition : public loka::app::scene::NodeDefinitionBase
  {
  public:
    FullRebuildLedgerDefinition(bool *useReplacement,
                                loka::app::scene::NodeDefinitionBase *initial,
                                loka::app::scene::NodeDefinitionBase *replacement)
        : useReplacement_(useReplacement),
          initial_(initial),
          replacement_(replacement)
    {
    }

    virtual loka::app::scene::Node *create() const
    {
      return this->selected()->create();
    }

    virtual loka::app::scene::Node *createInPlace(void *memory) const
    {
      return this->selected()->createInPlace(memory);
    }

    virtual size_t nodeSize() const
    {
      return this->selected()->nodeSize();
    }

    virtual size_t nodeAlign() const
    {
      return this->selected()->nodeAlign();
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return this->selected()->clone();
    }

    virtual loka::app::scene::NodeKind nodeKind() const
    {
      return this->selected()->nodeKind();
    }

    virtual const loka::app::scene::PropsBase *propsBase() const
    {
      return this->selected()->propsBase();
    }

    virtual bool hasEquivalentProps(
        const loka::app::scene::NodeDefinitionBase &other) const
    {
      return this->selected()->hasEquivalentProps(other);
    }

    virtual bool applyPropsToNode(loka::app::scene::Node *node) const
    {
      return this->selected()->applyPropsToNode(node);
    }

  private:
    loka::app::scene::NodeDefinitionBase *selected() const
    {
      return *this->useReplacement_ ? this->replacement_ : this->initial_;
    }

    bool *useReplacement_;
    loka::app::scene::NodeDefinitionBase *initial_;
    loka::app::scene::NodeDefinitionBase *replacement_;
  };

  struct IncompatibleParkedRootInputs
  {
    IncompatibleParkedRootInputs(loka::core::MutableState<bool> *visibleState,
                                 loka::core::MutableState<bool> *nestedConditionState,
                                 loka::core::MutableState<int> *revisionState,
                                 ParkedFactRecord *directRecord,
                                 ParkedFactRecord *nestedRecord,
                                 ParkedFactRecord *failedApplyOldRecord,
                                 ParkedFactRecord *failedApplyCurrentRecord,
                                 ParkedFactRecord *removedOldRecord)
        : visible(visibleState),
          nestedCondition(nestedConditionState),
          revision(revisionState),
          directOldRoot(directRecord),
          nestedOldRoot(nestedRecord),
          failedApplyOldRoot(failedApplyOldRecord),
          failedApplyCurrentRoot(failedApplyCurrentRecord),
          removedOldRoot(removedOldRecord)
    {
    }

    loka::core::MutableState<bool> *visible;
    loka::core::MutableState<bool> *nestedCondition;
    loka::core::MutableState<int> *revision;
    ParkedFactRecord *directOldRoot;
    ParkedFactRecord *nestedOldRoot;
    ParkedFactRecord *failedApplyOldRoot;
    ParkedFactRecord *failedApplyCurrentRoot;
    ParkedFactRecord *removedOldRoot;
  };

  IncompatibleParkedRootInputs *g_incompatibleParkedRootInputs = 0;

  class IncompatibleParkedRootBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<IncompatibleParkedRootBoundaryNode>
      IncompatibleParkedRootBoundaryProps;
  class IncompatibleParkedRootBoundaryNode
      : public PropsRecomposingBoundaryNode<IncompatibleParkedRootBoundaryNode,
                                            IncompatibleParkedRootBoundaryProps>
  {
  public:
    explicit IncompatibleParkedRootBoundaryNode(
        const IncompatibleParkedRootBoundaryProps &props)
        : PropsRecomposingBoundaryNode<IncompatibleParkedRootBoundaryNode,
                                       IncompatibleParkedRootBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_incompatibleParkedRootInputs &&
          g_incompatibleParkedRootInputs->revision)
      {
        registrar.markDirtyOnChange(g_incompatibleParkedRootInputs->revision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      const bool revised =
          g_incompatibleParkedRootInputs &&
          g_incompatibleParkedRootInputs->revision &&
          g_incompatibleParkedRootInputs->revision->get() != 0;

      ParkedFactDefinition directOld(
          (ParkedFactProps(g_incompatibleParkedRootInputs->directOldRoot)));
      loka::app::EditTextDefinition directCurrent;
      loka::app::ButtonDefinition directShown("incompatible-direct-shown");
      loka::app::scene::NodeDefinitionBase *directHidden =
          revised
              ? static_cast<loka::app::scene::NodeDefinitionBase *>(&directCurrent)
              : static_cast<loka::app::scene::NodeDefinitionBase *>(&directOld);
      loka::app::scene::ConditionalDefinition direct(
          (loka::app::scene::ConditionalProps(
              g_incompatibleParkedRootInputs->visible,
              &directShown,
              directHidden)));

      ParkedFactDefinition nestedOld(
          (ParkedFactProps(g_incompatibleParkedRootInputs->nestedOldRoot)));
      loka::app::EditTextDefinition nestedCurrent;
      loka::app::ButtonDefinition nestedAlternate("incompatible-nested-alternate");
      loka::app::scene::NodeDefinitionBase *nestedCurrentBranch =
          revised
              ? static_cast<loka::app::scene::NodeDefinitionBase *>(&nestedCurrent)
              : static_cast<loka::app::scene::NodeDefinitionBase *>(&nestedOld);
      loka::app::scene::ConditionalDefinition nested(
          (loka::app::scene::ConditionalProps(
              g_incompatibleParkedRootInputs->nestedCondition,
              &nestedAlternate,
              nestedCurrentBranch)));
      loka::app::FragmentDefinition outerParked;
      outerParked << nested;
      loka::app::ButtonDefinition outerShown("incompatible-outer-shown");
      loka::app::scene::ConditionalDefinition outer(
          (loka::app::scene::ConditionalProps(
              g_incompatibleParkedRootInputs->visible,
              &outerShown,
              &outerParked)));

      ParkedFactDefinition failedApplyOld(
          (ParkedFactProps(g_incompatibleParkedRootInputs->failedApplyOldRoot)));
      RetainedApplyFailureParkedFactDefinition failedApplyCurrent(
          g_incompatibleParkedRootInputs->failedApplyCurrentRoot);
      loka::app::ButtonDefinition failedApplyShown("failed-apply-shown");
      loka::app::scene::NodeDefinitionBase *failedApplyHidden =
          revised
              ? static_cast<loka::app::scene::NodeDefinitionBase *>(&failedApplyCurrent)
              : static_cast<loka::app::scene::NodeDefinitionBase *>(&failedApplyOld);
      loka::app::scene::ConditionalDefinition failedApply(
          (loka::app::scene::ConditionalProps(
              g_incompatibleParkedRootInputs->visible,
              &failedApplyShown,
              failedApplyHidden)));

      ParkedFactDefinition removedOld(
          (ParkedFactProps(g_incompatibleParkedRootInputs->removedOldRoot)));
      loka::app::ButtonDefinition removedShown("removed-branch-shown");
      loka::app::scene::ConditionalDefinition removed(
          (loka::app::scene::ConditionalProps(
              g_incompatibleParkedRootInputs->visible,
              &removedShown,
              revised
                  ? static_cast<loka::app::scene::NodeDefinitionBase *>(0)
                  : static_cast<loka::app::scene::NodeDefinitionBase *>(&removedOld))));

      loka::app::FragmentDefinition root;
      root << direct << outer << failedApply << removed;
      composition.declare(root);
    }
  };

  class IncompatibleParkedRootHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<IncompatibleParkedRootHarnessBoundaryNode>
      IncompatibleParkedRootHarnessBoundaryProps;
  class IncompatibleParkedRootHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<IncompatibleParkedRootHarnessBoundaryNode>
  {
  public:
    explicit IncompatibleParkedRootHarnessBoundaryNode(
        const IncompatibleParkedRootHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<IncompatibleParkedRootHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::scene::Boundary<IncompatibleParkedRootBoundaryNode>());
    }
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

  assert(seatRetained && contentFresh &&
         "the active branch exposes recomposed constant content after the pump settles");

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

  assert(seatRetained && contentFresh &&
         "the re-entered Conditional branch exposes current content in the same apply");

  scene.unmount();
  g_contentInputs = 0;
}

void testNestedConditionalSeatRepointsDefinitionsAtOuterReentry()
{
  ParkedFactRecord oldSourceRecord;
  ParkedFactRecord currentSourceRecord;
  ParkedFactRecord expiredSourceRecord;
  loka::core::MutableState<bool> outerCondition(false);
  loka::core::MutableState<bool> innerCondition(false);
  loka::core::MutableState<int> revision(0);
  NestedSeatReentryInputs inputs(&outerCondition,
                                 &innerCondition,
                                 &revision,
                                 &oldSourceRecord,
                                 &currentSourceRecord,
                                 &expiredSourceRecord);
  g_nestedSeatReentryInputs = &inputs;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<NestedSeatReentryHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  outerCondition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  outerCondition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  innerCondition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  assert(oldSourceRecord.constructionCount == 0);
  assert(expiredSourceRecord.constructionCount == 0);
  assert(currentSourceRecord.constructionCount == 1 &&
         "a nested seat flipped after re-entry reads the current definition generation");

  scene.unmount();
  g_nestedSeatReentryInputs = 0;
}

void testShowDslParkedBranchIsCurrentAtReentry()
{
  ParkedFactRecord oldSourceRecord;
  ParkedFactRecord currentSourceRecord;
  ParkedFactRecord expiredSourceRecord;
  loka::core::MutableState<bool> condition(true);
  loka::core::MutableState<int> revision(0);
  ShowReentryInputs inputs(&condition,
                           &revision,
                           &oldSourceRecord,
                           &currentSourceRecord,
                           &expiredSourceRecord);
  g_showReentryInputs = &inputs;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<ShowReentryHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(oldSourceRecord.constructionCount == 1);
  loka::app::scene::Node *branchBeforeHide = currentSourceRecord.node;
  assert(branchBeforeHide &&
         "Show exposes its active branch before the ledger round-trip");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(oldSourceRecord);

  revision.set(2);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  const NullScenePlatformController::LedgerRow *control =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(oldSourceRecord.constructionCount == 1 &&
         !recordedTransitionTo(oldSourceRecord, loka::app::scene::NODE_FACT_RETIRED, 0) &&
         "Show retains its seat and branch across the ledger round-trip");
  assert(currentSourceRecord.node == branchBeforeHide &&
         "Show preserves branch identity across hide and reentry");
  assert(currentSourceRecord.attachReads > 0 &&
         expiredSourceRecord.attachReads == 0 &&
         "Show reentry applies the current definition generation");
  assert(control && control->visible && control->handle && control->handle->owner &&
         control->handle->owner->lifetimeHint() == loka::app::scene::NATIVE_HINT_DESIRE_STAY &&
         "Show exposes current branch content in the reentry apply");

  scene.unmount();
  g_showReentryInputs = 0;
}

void testDepth2NestedConditionalSeatRepointsDefinitionsAtOuterReentry()
{
  ParkedFactRecord oldSourceRecord;
  ParkedFactRecord currentSourceRecord;
  ParkedFactRecord expiredSourceRecord;
  loka::core::MutableState<bool> outerCondition(false);
  loka::core::MutableState<bool> innerCondition(false);
  loka::core::MutableState<int> revision(0);
  NestedSeatReentryInputs inputs(&outerCondition,
                                 &innerCondition,
                                 &revision,
                                 &oldSourceRecord,
                                 &currentSourceRecord,
                                 &expiredSourceRecord);
  g_nestedSeatReentryInputs = &inputs;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<Depth2NestedSeatReentryHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  outerCondition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  outerCondition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  innerCondition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  assert(oldSourceRecord.constructionCount == 0);
  assert(expiredSourceRecord.constructionCount == 0);
  assert(currentSourceRecord.constructionCount == 1 &&
         "depth-2 recursive reentry reaches the nested seat's current definition");

  scene.unmount();
  g_nestedSeatReentryInputs = 0;
}

void testFullRebuildSubsumesParkedBranchLedgerGeneration()
{
  ParkedFactRecord record;
  loka::core::MutableState<bool> condition(false);
  bool useReplacement = false;
  ParkedFactDefinition parked((ParkedFactProps(&record)));
  loka::app::ButtonDefinition active("full-rebuild-active");
  loka::app::scene::ConditionalDefinition conditional(
      (loka::app::scene::ConditionalProps(&condition, &active, &parked)));
  loka::app::FragmentDefinition initial;
  initial << conditional;
  loka::app::EditTextDefinition replacement;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      new FullRebuildLedgerDefinition(&useReplacement, &initial, &replacement));
  mountAndAttach(scene, platform);

  assert(record.constructionCount == 1);
  condition.set(true);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(record);

  useReplacement = true;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(scene.flushInvalidation());
  assertParkedRetirementTransitionTable(record);

  scene.unmount();
}

void testIncompatibleParkedBranchRootsRetireAndRecreateAtReentry()
{
  ParkedFactRecord directOldRoot;
  ParkedFactRecord nestedOldRoot;
  ParkedFactRecord failedApplyOldRoot;
  ParkedFactRecord failedApplyCurrentRoot;
  ParkedFactRecord removedOldRoot;
  loka::core::MutableState<bool> visible(false);
  loka::core::MutableState<bool> nestedCondition(false);
  loka::core::MutableState<int> revision(0);
  IncompatibleParkedRootInputs inputs(&visible,
                                      &nestedCondition,
                                      &revision,
                                      &directOldRoot,
                                      &nestedOldRoot,
                                      &failedApplyOldRoot,
                                      &failedApplyCurrentRoot,
                                      &removedOldRoot);
  g_incompatibleParkedRootInputs = &inputs;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<IncompatibleParkedRootHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);

  assert(directOldRoot.constructionCount == 1);
  assert(nestedOldRoot.constructionCount == 1);
  assert(failedApplyOldRoot.constructionCount == 1);
  assert(failedApplyCurrentRoot.constructionCount == 0);
  assert(removedOldRoot.constructionCount == 1);

  visible.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(directOldRoot);
  assertParkedTransitionTable(nestedOldRoot);
  assertParkedTransitionTable(failedApplyOldRoot);
  assertParkedTransitionTable(removedOldRoot);

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  visible.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());

  std::size_t visibleEditRoots = 0;
  const std::vector<NullScenePlatformController::LedgerRow> &ledger = platform.ledger();
  for (std::size_t i = 0; i < ledger.size(); ++i)
  {
    if (ledger[i].recipe == NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT &&
        ledger[i].visible)
    {
      ++visibleEditRoots;
    }
  }
  assert(recordedTransitionTo(directOldRoot,
                              loka::app::scene::NODE_FACT_RETIRED,
                              0) &&
         recordedTransitionTo(nestedOldRoot,
                              loka::app::scene::NODE_FACT_RETIRED,
                              0) &&
         recordedTransitionTo(failedApplyOldRoot,
                              loka::app::scene::NODE_FACT_RETIRED,
                              0) &&
         failedApplyCurrentRoot.constructionCount == 1 &&
         recordedTransitionTo(removedOldRoot,
                              loka::app::scene::NODE_FACT_RETIRED,
                              0) &&
         visibleEditRoots == 2 &&
         "incompatible and failed-reconcile roots retire and recreate at reentry");

  scene.unmount();
  g_incompatibleParkedRootInputs = 0;
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

namespace
{
  class StdCompositionShowShapeTypeTag
  {
  };
  class StdCompositionShowShapeBoundaryNode;
  struct StdCompositionShowShapeProps
      : public loka::app::scene::NodePropsBase<StdCompositionShowShapeProps>
  {
    typedef StdCompositionShowShapeTypeTag TypeTag;
    typedef StdCompositionShowShapeBoundaryNode NodeType;
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return this->propsTypeId() < rhs.propsTypeId();
    }
  };
  StdCompositionShowShapeBoundaryNode *g_stdCompositionShowShapeNode = 0;
  /** Mirrors the SimpleViewer shape: a compose-once StdComposition boundary
      whose Show condition is a boundary-owned NodeState — the door must fire
      from the condition dirty alone, with no recomposition of the boundary. */
  class StdCompositionShowShapeBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<StdCompositionShowShapeProps>
  {
  public:
    explicit StdCompositionShowShapeBoundaryNode(const StdCompositionShowShapeProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<StdCompositionShowShapeProps>(props),
          dialogShown_()
    {
      this->state(this->dialogShown_, false);
      g_stdCompositionShowShapeNode = this;
    }
    virtual ~StdCompositionShowShapeBoundaryNode()
    {
      if (g_stdCompositionShowShapeNode == this)
      {
        g_stdCompositionShowShapeNode = 0;
      }
    }
    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::ButtonDefinition open("open");
      loka::app::EditTextDefinition dialog;
      loka::app::ShowDefinition shown = loka::app::Show(*this->dialogShown_.state());
      shown << dialog;
      loka::app::FragmentDefinition root;
      root << open << shown;
      c.declare(root);
    }
    void openDialog()
    {
      this->dialogShown_.set(true, true);
    }

  private:
    loka::app::scene::NodeState<bool> dialogShown_;
  };

  loka::core::MutableState<bool> *g_rootSeatRebuildCondition = 0;

  class RootSeatComposeOnceBranchBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RootSeatComposeOnceBranchBoundaryNode>
      RootSeatComposeOnceBranchProps;
  class RootSeatComposeOnceBranchBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<RootSeatComposeOnceBranchProps>
  {
  public:
    explicit RootSeatComposeOnceBranchBoundaryNode(
        const RootSeatComposeOnceBranchProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<RootSeatComposeOnceBranchProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      if (g_rootSeatRebuildCondition && g_rootSeatRebuildCondition->get())
      {
        loka::app::EditTextDefinition edit;
        composition.declare(edit);
        return;
      }
      loka::app::ButtonDefinition button("root-seat-rebuild-false");
      composition.declare(button);
    }
  };

  class RootSeatRebuildBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RootSeatRebuildBoundaryNode>
      RootSeatRebuildProps;
  class RootSeatRebuildBoundaryNode
      : public PropsRecomposingBoundaryNode<RootSeatRebuildBoundaryNode,
                                            RootSeatRebuildProps>
  {
  public:
    explicit RootSeatRebuildBoundaryNode(const RootSeatRebuildProps &props)
        : PropsRecomposingBoundaryNode<RootSeatRebuildBoundaryNode,
                                       RootSeatRebuildProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::scene::BoundaryDefinition<RootSeatComposeOnceBranchProps,
                                           RootSeatComposeOnceBranchBoundaryNode>
          whenTrue;
      loka::app::scene::BoundaryDefinition<RootSeatComposeOnceBranchProps,
                                           RootSeatComposeOnceBranchBoundaryNode>
          whenFalse;
      loka::app::scene::ConditionalDefinition seat(
          (loka::app::scene::ConditionalProps(g_rootSeatRebuildCondition,
                                              &whenTrue,
                                              &whenFalse)));
      // The seat itself is the root definition. Snapshot-mode UPDATE must use
      // the non-fast-path root rebuild resolver and attach the selected
      // compose-once branch through the real boundary door.
      composition.declare(seat);
    }
  };

  class RootSeatRebuildHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RootSeatRebuildHarnessBoundaryNode>
      RootSeatRebuildHarnessProps;
  class RootSeatRebuildHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<RootSeatRebuildHarnessBoundaryNode>
  {
  public:
    explicit RootSeatRebuildHarnessBoundaryNode(
        const RootSeatRebuildHarnessProps &props)
        : loka::app::scene::BoundaryNodeFor<RootSeatRebuildHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::scene::Boundary<RootSeatRebuildBoundaryNode>());
    }
  };
} // namespace

void testStdCompositionBoundaryShowFlipPreservesSiblings()
{
  NullScenePlatformController platform;
  loka::app::scene::NodeDefinition<StdCompositionShowShapeProps,
                                   StdCompositionShowShapeBoundaryNode> *root =
      new loka::app::scene::NodeDefinition<StdCompositionShowShapeProps,
                                           StdCompositionShowShapeBoundaryNode>(
          StdCompositionShowShapeProps());
  loka::app::scene::Scene scene(static_cast<loka::app::scene::NodeDefinitionBase *>(root));
  mountAndAttach(scene, platform);
  const NullScenePlatformController::LedgerRow *button =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  assert(button && button->visible);
  assert(!platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT));
  assert(g_stdCompositionShowShapeNode);

  g_stdCompositionShowShapeNode->openDialog();
  if (scene.hasPendingInvalidation())
  {
    assert(scene.flushInvalidation());
  }

  button = platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const NullScenePlatformController::LedgerRow *dialog =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(button && button->visible &&
         "siblings survive a Show flip inside a compose-once boundary");
  assert(dialog && dialog->visible &&
         "the shown branch materializes at the scheduled apply");
  scene.unmount();
}

void testComposeOnceBranchAtRootSeatSurvivesSnapshotRebuild()
{
  loka::core::MutableState<bool> condition(false);
  g_rootSeatRebuildCondition = &condition;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<RootSeatRebuildHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);
  const NullScenePlatformController::LedgerRow *button =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  assert(button && button->visible);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  const NullScenePlatformController::LedgerRow *edit =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(edit && edit->visible &&
         "root-seat rebuild attaches the selected compose-once branch");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  button = platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  assert(button && button->visible &&
         "root-seat rebuild reenters the retained compose-once branch");
  scene.unmount();
  g_rootSeatRebuildCondition = 0;
}

void testGenerationRetirementDoesNotLeaveStaleConditionalSeatMapping()
{
  ParkedFactRecord record;
  loka::core::MutableState<bool> condition(false);
  bool useReplacement = false;
  ParkedFactDefinition parked((ParkedFactProps(&record)));
  loka::app::ButtonDefinition active("stale-map-active");
  loka::app::scene::ConditionalDefinition conditional(
      (loka::app::scene::ConditionalProps(&condition, &active, &parked)));
  loka::app::FragmentDefinition conditionalRoot;
  conditionalRoot << conditional;
  loka::app::EditTextDefinition replacement;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      new FullRebuildLedgerDefinition(&useReplacement,
                                      &conditionalRoot,
                                      &replacement));
  mountAndAttach(scene, platform);

  condition.set(true);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(record);

  useReplacement = true;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(scene.flushInvalidation());
  assertParkedRetirementTransitionTable(record);

  useReplacement = false;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(scene.flushInvalidation());
  condition.set(false);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(scene.flushInvalidation());
  assert(record.constructionCount == 2 && record.node &&
         record.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "post-generation flip resolves only the recreated seat mapping");

  scene.unmount();
}

namespace
{
  struct PolicyDeliveryRecord
  {
    PolicyDeliveryRecord()
        : constructions(0),
          applies(0),
          value(-1),
          node(0)
    {
    }

    int constructions;
    int applies;
    int value;
    loka::app::scene::Node *node;
  };

  class PolicyDeliveryProbeNode;
  struct PolicyDeliveryProbeTypeTag
  {
  };
  struct PolicyDeliveryProbeProps
      : public loka::app::scene::NodePropsBase<PolicyDeliveryProbeProps>
  {
    typedef PolicyDeliveryProbeTypeTag TypeTag;
    typedef PolicyDeliveryProbeNode NodeType;

    PolicyDeliveryProbeProps(PolicyDeliveryRecord *recordValue = 0,
                             loka::core::State<int> *revisionValue = 0)
        : record(recordValue),
          revision(revisionValue)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const PolicyDeliveryProbeProps &other =
          static_cast<const PolicyDeliveryProbeProps &>(rhs);
      return this->record < other.record ||
             (this->record == other.record && this->revision < other.revision);
    }

    PolicyDeliveryRecord *record;
    loka::core::State<int> *revision;
  };

  class PolicyDeliveryProbeNode : public loka::app::scene::Node
  {
  public:
    typedef PolicyDeliveryProbeTypeTag TypeTag;

    explicit PolicyDeliveryProbeNode(const PolicyDeliveryProbeProps &propsValue)
        : props(propsValue)
    {
      if (this->props.record)
      {
        ++this->props.record->constructions;
        this->props.record->node = this;
        this->props.record->value =
            this->props.revision ? this->props.revision->get() : -1;
      }
    }

    PolicyDeliveryProbeProps props;
  };

  class PolicyDeliveryProbeDefinition
      : public loka::app::scene::NodeDefinition<PolicyDeliveryProbeProps,
                                                PolicyDeliveryProbeNode>
  {
  public:
    typedef loka::app::scene::NodeDefinition<PolicyDeliveryProbeProps,
                                              PolicyDeliveryProbeNode>
        BaseType;

    PolicyDeliveryProbeDefinition(PolicyDeliveryRecord *record,
                                  loka::core::State<int> *revision)
        : BaseType(PolicyDeliveryProbeProps(record, revision))
    {
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      PolicyDeliveryProbeDefinition *copy =
          new PolicyDeliveryProbeDefinition(this->props.record, this->props.revision);
      if (copy)
      {
        copy->copyTestIdPolicyFrom(*this);
      }
      return copy;
    }

    virtual bool applyPropsToNode(loka::app::scene::Node *node) const
    {
      if (!BaseType::applyPropsToNode(node))
      {
        return false;
      }
      PolicyDeliveryProbeNode *probe =
          static_cast<PolicyDeliveryProbeNode *>(node);
      if (probe->props.record)
      {
        ++probe->props.record->applies;
        probe->props.record->node = probe;
        probe->props.record->value =
            probe->props.revision ? probe->props.revision->get() : -1;
      }
      return true;
    }
  };

  loka::core::MutableState<bool> *g_policyDefaultCondition = 0;
  loka::core::MutableState<bool> *g_policyScopedCondition = 0;
  loka::core::MutableState<int> *g_policyRevision = 0;
  ParkedFactRecord *g_policyDefaultFact = 0;
  ParkedFactRecord *g_policyScopedFact = 0;
  ParkedFactRecord *g_policyScopedCurrentFact = 0;
  PolicyDeliveryRecord *g_policyDefaultDelivery = 0;
  PolicyDeliveryRecord *g_policyScopedDelivery = 0;

  class PolicyDestroyRecomposeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyDestroyRecomposeBoundaryNode>
      PolicyDestroyRecomposeBoundaryProps;
  class PolicyDestroyRecomposeBoundaryNode
      : public PropsRecomposingBoundaryNode<PolicyDestroyRecomposeBoundaryNode,
                                            PolicyDestroyRecomposeBoundaryProps>
  {
  public:
    explicit PolicyDestroyRecomposeBoundaryNode(
        const PolicyDestroyRecomposeBoundaryProps &props)
        : PropsRecomposingBoundaryNode<PolicyDestroyRecomposeBoundaryNode,
                                       PolicyDestroyRecomposeBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_policyRevision)
      {
        registrar.markDirtyOnChange(g_policyRevision, loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      ParkedFactDefinition defaultProbe((ParkedFactProps(g_policyDefaultFact)));
      loka::app::ButtonDefinition defaultControl("policy-default-retain");
      loka::app::FragmentDefinition defaultBranch;
      defaultBranch << defaultProbe << defaultControl;

      ParkedFactDefinition scopedProbe(
          (ParkedFactProps(g_policyRevision && g_policyRevision->get() != 0
                               ? g_policyScopedCurrentFact
                               : g_policyScopedFact)));
      loka::app::EditTextDefinition scopedControl;
      loka::app::PolicyScopeDefinition destroyScope;
      destroyScope.destroyOnDetach() << scopedProbe << scopedControl;

      loka::app::FragmentDefinition defaultHidden;
      loka::app::FragmentDefinition scopedHidden;
      loka::app::scene::ConditionalDefinition defaultSeat(
          (loka::app::scene::ConditionalProps(g_policyDefaultCondition,
                                              &defaultBranch,
                                              &defaultHidden)));
      loka::app::scene::ConditionalDefinition scopedSeat(
          (loka::app::scene::ConditionalProps(g_policyScopedCondition,
                                              &destroyScope,
                                              &scopedHidden)));
      loka::app::FragmentDefinition root;
      root << defaultSeat << scopedSeat;
      composition.declare(root);
    }
  };

  class PolicyDeliverRecomposeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyDeliverRecomposeBoundaryNode>
      PolicyDeliverRecomposeBoundaryProps;
  class PolicyDeliverRecomposeBoundaryNode
      : public PropsRecomposingBoundaryNode<PolicyDeliverRecomposeBoundaryNode,
                                            PolicyDeliverRecomposeBoundaryProps>
  {
  public:
    explicit PolicyDeliverRecomposeBoundaryNode(
        const PolicyDeliverRecomposeBoundaryProps &props)
        : PropsRecomposingBoundaryNode<PolicyDeliverRecomposeBoundaryNode,
                                       PolicyDeliverRecomposeBoundaryProps>(props)
    {
    }


    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_policyRevision)
      {
        registrar.markDirtyOnChange(g_policyRevision, loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      PolicyDeliveryProbeDefinition defaultProbe(g_policyDefaultDelivery,
                                                  g_policyRevision);
      loka::app::FragmentDefinition defaultBranch;
      defaultBranch << defaultProbe;

      PolicyDeliveryProbeDefinition scopedProbe(g_policyScopedDelivery,
                                                 g_policyRevision);
      loka::app::PolicyScopeDefinition deliverScope;
      deliverScope.deliverWhileDetached() << scopedProbe;

      loka::app::FragmentDefinition defaultHidden;
      loka::app::FragmentDefinition scopedHidden;
      loka::app::scene::ConditionalDefinition defaultSeat(
          (loka::app::scene::ConditionalProps(g_policyDefaultCondition,
                                              &defaultBranch,
                                              &defaultHidden)));
      loka::app::scene::ConditionalDefinition scopedSeat(
          (loka::app::scene::ConditionalProps(g_policyScopedCondition,
                                              &deliverScope,
                                              &scopedHidden)));
      loka::app::FragmentDefinition root;
      root << defaultSeat << scopedSeat;
      composition.declare(root);
    }
  };

  class PolicyDestroyHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyDestroyHarnessBoundaryNode>
      PolicyDestroyHarnessBoundaryProps;
  class PolicyDestroyHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<PolicyDestroyHarnessBoundaryNode>
  {
  public:
    explicit PolicyDestroyHarnessBoundaryNode(const PolicyDestroyHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<PolicyDestroyHarnessBoundaryNode>(props)
    {
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::scene::Boundary<PolicyDestroyRecomposeBoundaryNode>());
    }
  };

  class PolicyDeliverHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyDeliverHarnessBoundaryNode>
      PolicyDeliverHarnessBoundaryProps;
  class PolicyDeliverHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<PolicyDeliverHarnessBoundaryNode>
  {
  public:
    explicit PolicyDeliverHarnessBoundaryNode(const PolicyDeliverHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<PolicyDeliverHarnessBoundaryNode>(props)
    {
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::scene::Boundary<PolicyDeliverRecomposeBoundaryNode>());
    }
  };

  class PolicyDestroyStdBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyDestroyStdBoundaryNode>
      PolicyDestroyStdBoundaryProps;
  PolicyDestroyStdBoundaryNode *g_policyDestroyStdNode = 0;
  class PolicyDestroyStdBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<PolicyDestroyStdBoundaryProps>
  {
  public:
    explicit PolicyDestroyStdBoundaryNode(const PolicyDestroyStdBoundaryProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<PolicyDestroyStdBoundaryProps>(props),
          defaultShown_(),
          scopedShown_()
    {
      this->state(this->defaultShown_, true);
      this->state(this->scopedShown_, true);
      g_policyDestroyStdNode = this;
    }
    virtual ~PolicyDestroyStdBoundaryNode()
    {
      if (g_policyDestroyStdNode == this)
      {
        g_policyDestroyStdNode = 0;
      }
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      ParkedFactDefinition defaultProbe((ParkedFactProps(g_policyDefaultFact)));
      loka::app::ButtonDefinition defaultControl("policy-std-default");
      loka::app::FragmentDefinition defaultBranch;
      defaultBranch << defaultProbe << defaultControl;

      ParkedFactDefinition scopedProbe((ParkedFactProps(g_policyScopedFact)));
      loka::app::EditTextDefinition scopedControl;
      loka::app::PolicyScopeDefinition destroyScope;
      destroyScope.destroyOnDetach() << scopedProbe << scopedControl;

      loka::app::FragmentDefinition defaultHidden;
      loka::app::FragmentDefinition scopedHidden;
      loka::app::scene::ConditionalDefinition defaultSeat(
          (loka::app::scene::ConditionalProps(this->defaultShown_.state(),
                                              &defaultBranch,
                                              &defaultHidden)));
      loka::app::scene::ConditionalDefinition scopedSeat(
          (loka::app::scene::ConditionalProps(this->scopedShown_.state(),
                                              &destroyScope,
                                              &scopedHidden)));
      loka::app::FragmentDefinition root;
      root << defaultSeat << scopedSeat;
      composition.declare(root);
    }
    void hideBoth()
    {
      this->defaultShown_.set(false, true);
      this->scopedShown_.set(false, true);
    }
    void reshowScoped()
    {
      this->scopedShown_.set(true, true);
    }

  private:
    loka::app::scene::NodeState<bool> defaultShown_;
    loka::app::scene::NodeState<bool> scopedShown_;
  };

  class PolicyDeliverStdBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyDeliverStdBoundaryNode>
      PolicyDeliverStdBoundaryProps;
  PolicyDeliverStdBoundaryNode *g_policyDeliverStdNode = 0;
  class PolicyDeliverStdBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<PolicyDeliverStdBoundaryProps>
  {
  public:
    explicit PolicyDeliverStdBoundaryNode(const PolicyDeliverStdBoundaryProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<PolicyDeliverStdBoundaryProps>(props),
          defaultShown_(),
          scopedShown_(),
          revision_()
    {
      this->state(this->defaultShown_, true);
      this->state(this->scopedShown_, true);
      this->state(this->revision_, 0);
      g_policyDeliverStdNode = this;
    }
    virtual ~PolicyDeliverStdBoundaryNode()
    {
      if (g_policyDeliverStdNode == this)
      {
        g_policyDeliverStdNode = 0;
      }
    }
    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(this->revision_.state(),
                                  loka::app::scene::NODE_DIRTY_PROPS);
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      PolicyDeliveryProbeDefinition defaultProbe(g_policyDefaultDelivery,
                                                  this->revision_.state());
      loka::app::FragmentDefinition defaultBranch;
      defaultBranch << defaultProbe;

      PolicyDeliveryProbeDefinition scopedProbe(g_policyScopedDelivery,
                                                 this->revision_.state());
      loka::app::PolicyScopeDefinition deliverScope;
      deliverScope.deliverWhileDetached() << scopedProbe;

      loka::app::FragmentDefinition defaultHidden;
      loka::app::FragmentDefinition scopedHidden;
      loka::app::scene::ConditionalDefinition defaultSeat(
          (loka::app::scene::ConditionalProps(this->defaultShown_.state(),
                                              &defaultBranch,
                                              &defaultHidden)));
      loka::app::scene::ConditionalDefinition scopedSeat(
          (loka::app::scene::ConditionalProps(this->scopedShown_.state(),
                                              &deliverScope,
                                              &scopedHidden)));
      loka::app::FragmentDefinition root;
      root << defaultSeat << scopedSeat;
      composition.declare(root);
    }
    void hideBoth()
    {
      this->defaultShown_.set(false, true);
      this->scopedShown_.set(false, true);
    }
    void revise()
    {
      this->revision_.set(1, true);
    }

  private:
    loka::app::scene::NodeState<bool> defaultShown_;
    loka::app::scene::NodeState<bool> scopedShown_;
    loka::app::scene::NodeState<int> revision_;
  };

  class PolicyDefinitionOnlyBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyDefinitionOnlyBoundaryNode>
      PolicyDefinitionOnlyBoundaryProps;
  class PolicyDefinitionOnlyBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<PolicyDefinitionOnlyBoundaryNode>
  {
  public:
    explicit PolicyDefinitionOnlyBoundaryNode(const PolicyDefinitionOnlyBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<PolicyDefinitionOnlyBoundaryNode>(props),
          shown_()
    {
      this->state(this->shown_, true);
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::EditTextDefinition content;
      content.lifetimeHint(loka::app::scene::NATIVE_HINT_DESIRE_STAY);
      loka::app::PolicyScopeDefinition scope = loka::app::PolicyScope();
      scope.deliverWhileDetached() << content;
      loka::app::ShowDefinition seat = loka::app::Show(*this->shown_.state());
      seat << scope;
      loka::app::FragmentDefinition root;
      root << seat;
      composition.declare(root);
    }

  private:
    loka::app::scene::NodeState<bool> shown_;
  };

  void clearPolicyGlobals()
  {
    g_policyDefaultCondition = 0;
    g_policyScopedCondition = 0;
    g_policyRevision = 0;
    g_policyDefaultFact = 0;
    g_policyScopedFact = 0;
    g_policyScopedCurrentFact = 0;
    g_policyDefaultDelivery = 0;
    g_policyScopedDelivery = 0;
  }
} // namespace

void testPolicyScopeIsDefinitionOnlyAndPreservesContentNativeHint()
{
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<PolicyDefinitionOnlyBoundaryNode>()));
  mountAndAttach(scene, platform);

  assert(scene.liveNodeCount() == 4 &&
         "PolicyScope folds into the seat plan without a runtime node");
  const NullScenePlatformController::LedgerRow *content =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(content && content->handle && content->handle->owner &&
         content->handle->owner->lifetimeHint() ==
             loka::app::scene::NATIVE_HINT_DESIRE_STAY &&
         "NativeLifetimeHint remains content-owned rather than scope payload");

  loka::app::PolicyScopeDefinition policies;
  assert(!policies.branchPolicies().destroyOnDetach &&
         !policies.branchPolicies().deliverWhileDetached);
  policies.destroyOnDetach().deliverWhileDetached();
  assert(policies.branchPolicies().destroyOnDetach &&
         policies.branchPolicies().deliverWhileDetached &&
         "PolicyScope exposes only the branch lifecycle/diff policy payload");
  scene.unmount();
}

void testPolicyScopeRejectsNonBranchRootPlacement()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0)
  {
    loka::core::MutableState<bool> condition(true);
    loka::app::EditTextDefinition content;
    loka::app::PolicyScopeDefinition scope;
    scope.destroyOnDetach() << content;
    loka::app::FragmentDefinition misplacedBranch;
    misplacedBranch << scope;
    loka::app::FragmentDefinition hidden;
    loka::app::scene::ConditionalDefinition seat(
        (loka::app::scene::ConditionalProps(&condition, &misplacedBranch, &hidden)));
    loka::app::FragmentDefinition *root = new loka::app::FragmentDefinition();
    (*root) << seat;
    NullScenePlatformController platform;
    loka::app::scene::Scene scene(root);
    mountAndAttach(scene, platform);
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGABRT &&
         "PolicyScope is legal only as the immediate branch root");
#endif
}

void testPolicyScopeDestroyOnDetachContrastsWithDefaultInRecomposingBoundary()
{
  ParkedFactRecord defaultRecord;
  ParkedFactRecord scopedRecord;
  ParkedFactRecord scopedCurrentRecord;
  loka::core::MutableState<bool> defaultCondition(true);
  loka::core::MutableState<bool> scopedCondition(true);
  loka::core::MutableState<int> revision(0);
  g_policyDefaultCondition = &defaultCondition;
  g_policyScopedCondition = &scopedCondition;
  g_policyRevision = &revision;
  g_policyDefaultFact = &defaultRecord;
  g_policyScopedFact = &scopedRecord;
  g_policyScopedCurrentFact = &scopedCurrentRecord;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<PolicyDestroyHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 2);
  const unsigned long rowsBefore = platform.teardownCounters().rowRemoved;

  defaultCondition.set(false);
  scopedCondition.set(false);
  assert(scene.flushInvalidation());
  assertParkedTransitionTable(defaultRecord);
  assert(scopedRecord.constructionCount == 1 &&
         recordedTransitionTo(scopedRecord, loka::app::scene::NODE_FACT_RETIRED, 0));
  assert(platform.ledger().size() == 1 &&
         platform.teardownCounters().rowRemoved == rowsBefore + 1 &&
         "destroyOnDetach retires native ownership while default parks it");

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  scene.flushInvalidation();
  scopedCondition.set(true);
  assert(scene.flushInvalidation());
  const NullScenePlatformController::LedgerRow *rebuilt =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(scopedRecord.constructionCount == 1 &&
         "destroyOnDetach does not reconstruct the expired branch definition");
  assert(scopedCurrentRecord.constructionCount == 1 &&
         "destroyOnDetach reshow constructs the current branch definition");
  assert(rebuilt && rebuilt->visible && rebuilt->handle && rebuilt->handle->owner &&
         "destroyOnDetach reshow projects the fresh branch");
  scene.unmount();
  clearPolicyGlobals();
}

void testPolicyScopeDeliverWhileDetachedContrastsWithDefaultInRecomposingBoundary()
{
  PolicyDeliveryRecord defaultRecord;
  PolicyDeliveryRecord scopedRecord;
  loka::core::MutableState<bool> defaultCondition(true);
  loka::core::MutableState<bool> scopedCondition(true);
  loka::core::MutableState<int> revision(0);
  g_policyDefaultCondition = &defaultCondition;
  g_policyScopedCondition = &scopedCondition;
  g_policyRevision = &revision;
  g_policyDefaultDelivery = &defaultRecord;
  g_policyScopedDelivery = &scopedRecord;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<PolicyDeliverHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);
  defaultCondition.set(false);
  scopedCondition.set(false);
  assert(scene.flushInvalidation());
  const int defaultAppliesBefore = defaultRecord.applies;
  const int scopedAppliesBefore = scopedRecord.applies;

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  scene.flushInvalidation();
  assert(defaultRecord.value == 0 &&
         defaultRecord.applies == defaultAppliesBefore &&
         "default policy defers parked-child reconciliation");
  assert(scopedRecord.value == 1 &&
         scopedRecord.applies > scopedAppliesBefore &&
         "deliverWhileDetached brings parked children current while hidden");
  scene.unmount();
  clearPolicyGlobals();
}

void testPolicyScopeDestroyOnDetachWorksInComposeOnceBoundary()
{
  ParkedFactRecord defaultRecord;
  ParkedFactRecord scopedRecord;
  g_policyDefaultFact = &defaultRecord;
  g_policyScopedFact = &scopedRecord;
  NullScenePlatformController platform;
  loka::app::scene::NodeDefinition<PolicyDestroyStdBoundaryProps,
                                   PolicyDestroyStdBoundaryNode> *root =
      new loka::app::scene::NodeDefinition<PolicyDestroyStdBoundaryProps,
                                           PolicyDestroyStdBoundaryNode>(
          PolicyDestroyStdBoundaryProps());
  loka::app::scene::Scene scene(root);
  mountAndAttach(scene, platform);
  assert(g_policyDestroyStdNode);
  const unsigned long rowsBefore = platform.teardownCounters().rowRemoved;

  g_policyDestroyStdNode->hideBoth();
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  assertParkedTransitionTable(defaultRecord);
  assert(recordedTransitionTo(scopedRecord, loka::app::scene::NODE_FACT_RETIRED, 0));
  assert(platform.ledger().size() == 1 &&
         platform.teardownCounters().rowRemoved == rowsBefore + 1);

  g_policyDestroyStdNode->reshowScoped();
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  assert(scopedRecord.constructionCount == 2 &&
         "compose-once destroyOnDetach reshow creates a fresh branch");
  scene.unmount();
  clearPolicyGlobals();
}

void testPolicyScopeDeliverWhileDetachedWorksInComposeOnceBoundary()
{
  PolicyDeliveryRecord defaultRecord;
  PolicyDeliveryRecord scopedRecord;
  g_policyDefaultDelivery = &defaultRecord;
  g_policyScopedDelivery = &scopedRecord;
  NullScenePlatformController platform;
  loka::app::scene::NodeDefinition<PolicyDeliverStdBoundaryProps,
                                   PolicyDeliverStdBoundaryNode> *root =
      new loka::app::scene::NodeDefinition<PolicyDeliverStdBoundaryProps,
                                           PolicyDeliverStdBoundaryNode>(
          PolicyDeliverStdBoundaryProps());
  loka::app::scene::Scene scene(root);
  mountAndAttach(scene, platform);
  assert(g_policyDeliverStdNode);
  g_policyDeliverStdNode->hideBoth();
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  const int defaultAppliesBefore = defaultRecord.applies;
  const int scopedAppliesBefore = scopedRecord.applies;

  g_policyDeliverStdNode->revise();
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  assert(defaultRecord.value == 0 &&
         defaultRecord.applies == defaultAppliesBefore);
  assert(scopedRecord.value == 1 &&
         scopedRecord.applies > scopedAppliesBefore &&
         "compose-once delivery reconciles the parked child at the door");
  scene.unmount();
  clearPolicyGlobals();
}

namespace
{
  class Step4ShapeTypeTag
  {
  };
  class Step4ShapeBoundaryNode;
  struct Step4ShapeProps : public loka::app::scene::NodePropsBase<Step4ShapeProps>
  {
    typedef Step4ShapeTypeTag TypeTag;
    typedef Step4ShapeBoundaryNode NodeType;
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return this->propsTypeId() < rhs.propsTypeId();
    }
  };
  Step4ShapeBoundaryNode *g_step4ShapeNode = 0;
  /** Mirrors Tutorial Step 4 with the maintainer's repro edit: a compose-once
      boundary whose Show branch holds anonymous mixed siblings (text, button)
      plus nested conditional seats. The Toolbox roulette is this shape failing
      to settle: every apply keeps churning structure. */
  class Step4ShapeBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<Step4ShapeProps>
  {
  public:
    explicit Step4ShapeBoundaryNode(const Step4ShapeProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<Step4ShapeProps>(props),
          showSummary_(),
          showItem1_(),
          showItem2_()
    {
      this->state(this->showSummary_, false);
      this->state(this->showItem1_, true);
      this->state(this->showItem2_, false);
      g_step4ShapeNode = this;
    }
    virtual ~Step4ShapeBoundaryNode()
    {
      if (g_step4ShapeNode == this)
      {
        g_step4ShapeNode = 0;
      }
    }
    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::ButtonDefinition addOutside("add");
      loka::app::ButtonDefinition toggle("toggle");
      loka::app::EditTextDefinition summaryText;
      loka::app::ButtonDefinition addInside("add");
      loka::app::EditTextDefinition item1;
      loka::app::EditTextDefinition item2;
      loka::app::ShowDefinition inner1 = loka::app::Show(*this->showItem1_.state());
      inner1 << item1;
      loka::app::ShowDefinition inner2 = loka::app::Show(*this->showItem2_.state());
      inner2 << item2;
      loka::app::ShowDefinition summary = loka::app::Show(*this->showSummary_.state());
      summary << summaryText << addInside << inner1 << inner2;
      loka::app::FragmentDefinition root;
      root << addOutside << toggle << summary;
      c.declare(root);
    }
    void toggleSummary(bool value)
    {
      this->showSummary_.set(value, true);
    }

  private:
    loka::app::scene::NodeState<bool> showSummary_;
    loka::app::scene::NodeState<bool> showItem1_;
    loka::app::scene::NodeState<bool> showItem2_;
  };
} // namespace

void testStep4ShapeSettlesAfterShowFlip()
{
  NullScenePlatformController platform;
  loka::app::scene::NodeDefinition<Step4ShapeProps, Step4ShapeBoundaryNode> *root =
      new loka::app::scene::NodeDefinition<Step4ShapeProps, Step4ShapeBoundaryNode>(
          Step4ShapeProps());
  loka::app::scene::Scene scene(static_cast<loka::app::scene::NodeDefinitionBase *>(root));
  mountAndAttach(scene, platform);
  assert(g_step4ShapeNode);

  g_step4ShapeNode->toggleSummary(true);
  if (scene.hasPendingInvalidation())
  {
    assert(scene.flushInvalidation());
  }
  assert(!scene.hasPendingInvalidation() &&
         "a single Show flip settles in one scheduled apply");

  NativeContextCallCounts afterShow(platform);
  for (int i = 0; i < 4; ++i)
  {
    requestChildPump(scene);
    assert(!scene.hasPendingInvalidation() &&
           "a re-pump with no state change must not re-arm invalidation");
    NativeContextCallCounts rePumped(platform);
    assert(rePumped == afterShow &&
           "a re-pump with no state change must not churn native structure");
  }

  g_step4ShapeNode->toggleSummary(false);
  if (scene.hasPendingInvalidation())
  {
    assert(scene.flushInvalidation());
  }
  NativeContextCallCounts afterHide(platform);
  for (int i = 0; i < 4; ++i)
  {
    requestChildPump(scene);
    NativeContextCallCounts rePumped(platform);
    assert(rePumped == afterHide &&
           "re-pumps while hidden must not churn native structure");
  }

  g_step4ShapeNode->toggleSummary(true);
  if (scene.hasPendingInvalidation())
  {
    assert(scene.flushInvalidation());
  }
  scene.unmount();
}

namespace
{
  struct RemovedConditionalSeatInputs
  {
    RemovedConditionalSeatInputs(loka::core::MutableState<bool> *conditionState,
                                 loka::core::MutableState<int> *revisionState,
                                 ParkedFactRecord *trueRecord,
                                 ParkedFactRecord *falseRecord)
        : condition(conditionState),
          revision(revisionState),
          whenTrue(trueRecord),
          whenFalse(falseRecord)
    {
    }

    loka::core::MutableState<bool> *condition;
    loka::core::MutableState<int> *revision;
    ParkedFactRecord *whenTrue;
    ParkedFactRecord *whenFalse;
  };

  RemovedConditionalSeatInputs *g_removedConditionalSeatInputs = 0;

  class RemovedConditionalSeatBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RemovedConditionalSeatBoundaryNode>
      RemovedConditionalSeatBoundaryProps;
  class RemovedConditionalSeatBoundaryNode
      : public PropsRecomposingBoundaryNode<RemovedConditionalSeatBoundaryNode,
                                            RemovedConditionalSeatBoundaryProps>
  {
  public:
    explicit RemovedConditionalSeatBoundaryNode(
        const RemovedConditionalSeatBoundaryProps &props)
        : PropsRecomposingBoundaryNode<RemovedConditionalSeatBoundaryNode,
                                       RemovedConditionalSeatBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_removedConditionalSeatInputs &&
          g_removedConditionalSeatInputs->revision)
      {
        registrar.markDirtyOnChange(g_removedConditionalSeatInputs->revision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      const bool seatPresent =
          g_removedConditionalSeatInputs &&
          g_removedConditionalSeatInputs->revision &&
          g_removedConditionalSeatInputs->revision->get() != 1;
      if (seatPresent)
      {
        ParkedFactDefinition whenTrue(
            (ParkedFactProps(g_removedConditionalSeatInputs->whenTrue)));
        ParkedFactDefinition whenFalse(
            (ParkedFactProps(g_removedConditionalSeatInputs->whenFalse)));
        loka::app::scene::ConditionalDefinition seat(
            (loka::app::scene::ConditionalProps(
                g_removedConditionalSeatInputs->condition,
                &whenTrue,
                &whenFalse)));
        seat.setNodeTag(401);
        root << seat;
      }
      composition.declare(root);
    }
  };

  class RemovedConditionalSeatHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RemovedConditionalSeatHarnessBoundaryNode>
      RemovedConditionalSeatHarnessBoundaryProps;
  class RemovedConditionalSeatHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<RemovedConditionalSeatHarnessBoundaryNode>
  {
  public:
    explicit RemovedConditionalSeatHarnessBoundaryNode(
        const RemovedConditionalSeatHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<RemovedConditionalSeatHarnessBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::scene::Boundary<RemovedConditionalSeatBoundaryNode>());
    }
  };
} // namespace

void testRemovedConditionalSeatReaddsFreshRuntimeAndBranches()
{
  loka::core::MutableState<bool> condition(true);
  loka::core::MutableState<int> revision(0);
  ParkedFactRecord whenTrue;
  ParkedFactRecord whenFalse;
  RemovedConditionalSeatInputs inputs(&condition,
                                      &revision,
                                      &whenTrue,
                                      &whenFalse);
  g_removedConditionalSeatInputs = &inputs;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<RemovedConditionalSeatHarnessBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(whenTrue.constructionCount == 1 &&
         whenFalse.constructionCount == 0);

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(whenTrue.constructionCount == 1 &&
         whenFalse.constructionCount == 1 &&
         "both pre-removal branches materialize exactly once");

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  assert(!scene.hasPendingInvalidation() &&
         "seat retirement drains before the same value key is re-added");

  revision.set(2);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(whenTrue.constructionCount == 2 && whenTrue.node &&
         whenTrue.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "re-adding a dead seat must materialize a fresh active branch");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(whenFalse.constructionCount == 2 && whenFalse.node &&
         whenFalse.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "seat death must also discard the old parked branch");

  scene.unmount();
  g_removedConditionalSeatInputs = 0;
}

namespace
{
  loka::core::MutableState<bool> *g_nullConditionalBranchCondition = 0;
  ParkedFactRecord *g_nullConditionalBranchRecord = 0;

  class NullConditionalBranchBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<NullConditionalBranchBoundaryNode>
      NullConditionalBranchBoundaryProps;
  class NullConditionalBranchBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<NullConditionalBranchBoundaryNode>
  {
  public:
    explicit NullConditionalBranchBoundaryNode(
        const NullConditionalBranchBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<NullConditionalBranchBoundaryNode>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      ParkedFactDefinition shown(
          (ParkedFactProps(g_nullConditionalBranchRecord)));
      loka::app::scene::ConditionalDefinition seat(
          (loka::app::scene::ConditionalProps(
              g_nullConditionalBranchCondition,
              &shown,
              0)));
      seat.setNodeTag(501);
      loka::app::FragmentDefinition root;
      root << seat;
      composition.declare(root);
    }
  };

  void clearNullConditionalBranchGlobals()
  {
    g_nullConditionalBranchCondition = 0;
    g_nullConditionalBranchRecord = 0;
  }
} // namespace

void testConditionalSeatInitiallyNullCanMaterialize()
{
  loka::core::MutableState<bool> condition(false);
  ParkedFactRecord shown;
  g_nullConditionalBranchCondition = &condition;
  g_nullConditionalBranchRecord = &shown;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<NullConditionalBranchBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(shown.constructionCount == 0);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(shown.constructionCount == 1 && shown.node &&
         shown.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "a seat mounted on its null side must materialize when shown");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(shown.node->lifecycleFact() ==
             loka::app::scene::NODE_FACT_DETACHED_RETAINED &&
         "the materialized branch parks when returning to the null side");

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(shown.constructionCount == 1 && shown.node->lifecycleFact() ==
                                             loka::app::scene::NODE_FACT_ATTACHED &&
         "the initially-null seat remains live across later flips");

  scene.unmount();
  clearNullConditionalBranchGlobals();
}

void testNullConditionalBranchParksAndReentersShownBranch()
{
  loka::core::MutableState<bool> condition(true);
  ParkedFactRecord shown;
  g_nullConditionalBranchCondition = &condition;
  g_nullConditionalBranchRecord = &shown;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<NullConditionalBranchBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(shown.constructionCount == 1 && shown.node);
  loka::app::scene::Node *original = shown.node;

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(original->lifecycleFact() ==
             loka::app::scene::NODE_FACT_DETACHED_RETAINED &&
         "flipping to a null branch must park the shown branch");

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  assert(scene.flushInvalidation());
  assert(shown.constructionCount == 1 && shown.node == original &&
         original->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "the shown branch must reenter from the null side without reconstruction");

  scene.unmount();
  clearNullConditionalBranchGlobals();
}
