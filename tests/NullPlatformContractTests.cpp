#include "NullPlatformContractTests.hpp"
#include "support/TestVerify.hpp"

#include <cassert>

#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/ComposableNode.hpp"
#include "app/scene/node/Conditional.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/NullWindow.hpp"
#include "platform/null/context/NullScrollBarContext.hpp"
#include "support/FullRebuildLedgerDefinition.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/RecomposingBoundary.hpp"

namespace
{
  using SceneTestSupport::FullRebuildLedgerDefinition;

  loka::core::MutableState<bool> *g_toggleVisible = 0;
  loka::app::scene::NativeLifetimeHint g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;

  class UnregisteredProjectionNode : public loka::app::scene::Node
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      static const char key = 0;
      return &key;
    }
  };

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
          hitRouteRemoved(platform.teardownCounters().hitRouteRemoved),
          queuedForNativeRetirement(platform.teardownCounters().queuedForNativeRetirement),
          shown(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN)),
          hidden(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN))
    {
    }

    bool operator==(const NativeContextCallCounts &other) const
    {
      return this->created == other.created && this->disposed == other.disposed
             && this->backPointerCleared == other.backPointerCleared && this->hitRouteRemoved == other.hitRouteRemoved
             && this->queuedForNativeRetirement == other.queuedForNativeRetirement && this->shown == other.shown
             && this->hidden == other.hidden;
    }

    unsigned long created;
    unsigned long disposed;
    unsigned long backPointerCleared;
    unsigned long hitRouteRemoved;
    unsigned long queuedForNativeRetirement;
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
    (void)record;
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
    (void)record;
    assert(record.constructionCount == 1);
    assert(record.attachReads == 1);
    assert(record.transitions.size() == 2);
    assert(record.transitions[0].previous == loka::app::scene::NODE_FACT_ATTACHED);
    assert(record.transitions[0].next == loka::app::scene::NODE_FACT_DETACHED_RETAINED);
    assert(record.transitions[1].previous == loka::app::scene::NODE_FACT_DETACHED_RETAINED);
    assert(record.transitions[1].next == loka::app::scene::NODE_FACT_RETIRED);
  }

  void requestChildPump(loka::app::scene::Scene &scene, NullScenePlatformController &platform)
  {
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
    platform.drainNativeRetirements();
  }

  void synchronizeThenDrainAtSafePoint(NullScenePlatformController &platform)
  {
    platform.synchronize();
    assert(platform.hasPendingSync() && "native retirement waits for the App safe point, not projection sync");
    platform.drainNativeRetirements();
  }

  void mountAndAttach(loka::app::scene::Scene &scene, NullScenePlatformController &platform)
  {
    scene.mount(&platform);
    scene.updateAttached(true);
  }

  void retireProjectedContextsWithoutApply(loka::app::scene::Scene &scene,
                                           NullScenePlatformController &platform)
  {
    // Stop at the detach line: unlike onChange()/destroy(), these two doors
    // queue native intake without flushing it before synchronize() can run.
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
    assert(root && "a mounted scene must have a root before native context retirement");
    loka::app::scene::LifecycleFactTestAccess::MarkSubtreeRetired(root);
    platform.releaseNodeContexts(root);
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
          (void)activeSafePointSequence;
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

void testNullNodeHandlerRefusalIsTypedObservableAndContextless()
{
  NullScenePlatformController platform;
  loka::app::CellNode cell((loka::app::CellProps()));
  loka::app::scene::LayoutState state;
  state.width = 100;
  state.height = 20;

  LOKA_VERIFY(!platform.prepareProjectedLayout(&cell, state));
  LOKA_VERIFY(!cell.getContext());
  assert(platform.cellRefusalCount() == 1);
}

void testNullNodeHandlerRealKindStillProjects()
{
  NullScenePlatformController platform;
  loka::app::ButtonNode button((loka::app::ButtonProps()));
  loka::app::scene::LayoutState state;
  state.width = 100;
  state.height = 20;

  LOKA_VERIFY(platform.prepareProjectedLayout(&button, state));
  LOKA_VERIFY(button.getContext());
}

void testNullNodeHandlerRegistryMissEducatesInDiagnosticBuilds()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0)
  {
    NullScenePlatformController platform;
    UnregisteredProjectionNode node;
    loka::app::scene::LayoutState state;
    platform.prepareProjectedLayout(&node, state);
    _exit(0);
  }
  int status = 0;
  LOKA_VERIFY(waitpid(child, &status, 0) == child);
  LOKA_VERIFY(WIFSIGNALED(status));
  LOKA_VERIFY(WTERMSIG(status) == SIGABRT &&
         "a registry miss must educate instead of sharing the typed-refusal path");
#endif
}

void testNullPlatformContract_A1_terminalFactRunsTeardownSequence()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  visible.set(false);
  requestChildPump(scene, platform);

  assert(platform.ledger().empty());
  assert(platform.teardownCounters().backPointerCleared == 1);
  assert(platform.teardownCounters().hitRouteRemoved == 1);
  assert(platform.teardownCounters().queuedForNativeRetirement == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 1);
  g_toggleVisible = 0;
}

void testNullPlatformContract_A1_safePointDrainsTeardownIntoPool()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  retireProjectedContextsWithoutApply(scene, platform);
  assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
  synchronizeThenDrainAtSafePoint(platform);

  assert(platform.ledger().empty());
  assert(platform.teardownCounters().backPointerCleared == 1);
  assert(platform.teardownCounters().hitRouteRemoved == 1);
  assert(platform.teardownCounters().queuedForNativeRetirement == 1);
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
  requestChildPump(scene, platform);

  assert(platform.ledger().size() == 1);
  assert(!platform.ledger()[0].visible);
  assert(platform.teardownCounters().backPointerCleared == 0);
  assert(platform.teardownCounters().hitRouteRemoved == 0);
  assert(platform.teardownCounters().queuedForNativeRetirement == 0);
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
  requestChildPump(scene, platform);

  assert(platform.intakeCheckFailCount() == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0);
  (void)handle;
  assert(!handle->disposed);
  assert(handle->leakedDeliberately);
  assert(platform.disposedCount() == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED) == 0);
  g_toggleVisible = 0;
}

void testNullPlatformContract_A3_safePointDrainsIntakeRefusal()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 1);
  NullScenePlatformController::FakeControlHandle *handle = platform.ledger()[0].handle;
  (void)handle;

  platform.preserveNextRetiredOwnerForTesting();
  retireProjectedContextsWithoutApply(scene, platform);
  assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
  synchronizeThenDrainAtSafePoint(platform);

  assert(platform.intakeCheckFailCount() == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0);
  assert(!handle->disposed);
  assert(handle->leakedDeliberately);
  assert(platform.disposedCount() == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED) == 0);
  g_toggleVisible = 0;
}

namespace
{
  void assertNullControlDetachWindow(loka::app::scene::Node &node, NullScenePlatformController::ControlRecipe recipe)
  {
    NullScenePlatformController platform;
    loka::app::scene::LayoutState state;
    state.width = 100;
    state.height = 20;
    LOKA_VERIFY(platform.prepareProjectedLayout(&node, state));

    const NullScenePlatformController::LedgerRow *attached = platform.findLedgerRow(recipe);
    (void)attached;
    assert(attached && attached->visible);
    assert(platform.hasHitTarget(recipe));
    LOKA_VERIFY(platform.injectNotification(recipe));
    const unsigned long deliveriesBeforeRetire = platform.injectedDeliveryCount();
    (void)deliveriesBeforeRetire;

    loka::app::scene::LifecycleFactTestAccess::MarkSubtreeRetired(&node);
    platform.releaseNodeContexts(&node);

    const NullScenePlatformController::LedgerRow *detached = platform.findLedgerRow(recipe);
    (void)detached;
    assert(detached && !detached->visible && "the native stays ledger-visible but hidden throughout the detach window");
    assert(!platform.hasHitTarget(recipe) && "terminal delivery removes the native from hit routing synchronously");
    LOKA_VERIFY(!platform.injectNotification(recipe) && platform.injectedDeliveryCount() == deliveriesBeforeRetire
                && "an injected notification reaches nothing in Loka after detach");
    assert(platform.hasPendingSync() && "the detached native remains queued until the platform safe point");
    assert(platform.retiredCount() == 1);
    assert(platform.disposedCount() == 0);

    platform.drainNativeRetirements();
    assert(!platform.hasPendingSync());
    assert(platform.ledger().empty());
    assert(platform.bucketStats(recipe).depth == 1);
    assertDisposalsAreInsideSafePoints(platform);
  }
} // namespace

void testNullPlatformContract_A4_buttonDetachWindowIsSilentUntilSafePoint()
{
  loka::app::ButtonNode button((loka::app::ButtonProps()));
  assertNullControlDetachWindow(button, NullScenePlatformController::CONTROL_RECIPE_BUTTON);
}

void testNullPlatformContract_A4_editTextDetachWindowIsSilentUntilSafePoint()
{
  loka::app::EditTextNode editText((loka::app::EditTextProps()));
  assertNullControlDetachWindow(editText, NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
}

void testNullPlatformContract_A4_scrollBarDetachWindowIsSilentUntilSafePoint()
{
  loka::app::ScrollBarNode scrollBar((loka::app::ScrollBarProps()));
  assertNullControlDetachWindow(scrollBar, NullScenePlatformController::CONTROL_RECIPE_SCROLL_BAR);
}

void testNullPlatformContract_A5_windowFlushDrainsNativeRetirementsAtReclaimBoundary()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_EAGER_RELEASE;
  NullPlatformContext context;
  NullScenePlatformController platform;
  WindowProps props;
  props.scene(new loka::app::scene::Scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>())));
  {
    NullWindow window(&context, props, &platform);
    loka::app::scene::Scene *scene = window.scene();
    assert(scene);
    scene->updateAttached(true);
    visible.set(false);
    scene->requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);

    LOKA_VERIFY(window.flushSceneInvalidation());

    assert(!platform.hasPendingSync() && "Window drains native retirement beside scene reclaim");
    assert(platform.disposedCount() == 1);
    assert(platform.ledger().empty());
    assertDisposalsAreInsideSafePoints(platform);
  }
  g_toggleVisible = 0;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
}

void testNullPlatformContract_B1_attachShowsControl()
{
  loka::app::ButtonDefinition button("shown");
  NullScenePlatformController platform;
  loka::app::scene::NodeDefinitionBase *rootDefinition = button.clone();
  LOKA_VERIFY(rootDefinition != 0);
  loka::app::scene::Scene scene(rootDefinition);
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
  requestChildPump(scene, platform);

  assert(platform.ledger().size() == 1);
  (void)handleId;
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
  requestChildPump(scene, platform);
  condition.set(true);
  requestChildPump(scene, platform);

  assert(platform.ledger().size() == 1);
  (void)handleId;
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
  requestChildPump(scene, platform);

  assert(platform.ledger().empty());
  assert(platform.teardownCounters().hitRouteRemoved == 1);
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
  requestChildPump(scene, platform);
  const unsigned long shownBefore = platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN);
  const unsigned long hiddenBefore = platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN);
  const unsigned long createdBefore = platform.eventCount(NullScenePlatformController::EVENT_CONTROL_CREATED);
  const unsigned long disposedBefore = platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED);

  innerCondition.set(false);
  requestChildPump(scene, platform);

  (void)shownBefore;
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN) == shownBefore);
  (void)hiddenBefore;
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN) == hiddenBefore);
  (void)createdBefore;
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_CREATED) == createdBefore);
  (void)disposedBefore;
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
    requestChildPump(scene, platform);
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
    requestChildPump(scene, platform);
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
    requestChildPump(scene, platform);
    assert(platform.disposedCount() == 0);
    assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 1);
  }
  g_toggleVisible = 0;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
}

void testNullPlatformContract_C2_safePointDrainsHintPolicy()
{
  {
    loka::core::MutableState<bool> visible(true);
    g_toggleVisible = &visible;
    g_toggleHint = loka::app::scene::NATIVE_HINT_EAGER_RELEASE;
    NullScenePlatformController platform;
    loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
    mountAndAttach(scene, platform);
    retireProjectedContextsWithoutApply(scene, platform);
    assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
    synchronizeThenDrainAtSafePoint(platform);
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
    retireProjectedContextsWithoutApply(scene, platform);
    assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
    synchronizeThenDrainAtSafePoint(platform);
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
    retireProjectedContextsWithoutApply(scene, platform);
    assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
    synchronizeThenDrainAtSafePoint(platform);
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
  requestChildPump(scene, platform);

  visible.set(false);
  requestChildPump(scene, platform);
  assert(platform.disposedCount() >= 1 &&
         "the retire flush honors the runtime hint change: EAGER_RELEASE disposes");
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0 &&
         "nothing pools under the fresh hint");

  scene.unmount();
  g_toggleVisible = 0;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
}

void testNullPlatformContract_C3_safePointDrainsFreshHint()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DESIRE_STAY;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  g_toggleHint = loka::app::scene::NATIVE_HINT_EAGER_RELEASE;
  requestChildPump(scene, platform);
  retireProjectedContextsWithoutApply(scene, platform);
  assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
  synchronizeThenDrainAtSafePoint(platform);

  assert(platform.disposedCount() >= 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0);
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
  requestChildPump(scene, platform);
  assert(platform.ledger().size() == 1);
  const int editTextId = platform.ledger()[0].handle->id;
  (void)buttonId;
  (void)editTextId;
  assert(editTextId != buttonId);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT).missCount == 1);

  mode.set(1);
  requestChildPump(scene, platform);
  assert(platform.ledger()[0].handle->id == buttonId);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).hitCount == 1);

  mode.set(2);
  requestChildPump(scene, platform);
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
    requestChildPump(scene, platform);
    visible.set(true);
    requestChildPump(scene, platform);
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
  requestChildPump(scene, platform);

  NullScenePlatformController::BucketStats stats =
      platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  (void)stats;
  assert(stats.depth == 1);
  assert(stats.evictCount == 1);
  assert(platform.disposedCount() == 1);
  g_multipleVisible = 0;
}

void testNullPlatformContract_D3_safePointDrainsDepthCapEviction()
{
  loka::core::MutableState<bool> visible(true);
  g_multipleVisible = &visible;
  NullScenePlatformController platform(1);
  loka::app::scene::Scene scene((loka::app::scene::Boundary<MultipleButtonBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 2);

  retireProjectedContextsWithoutApply(scene, platform);
  assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
  synchronizeThenDrainAtSafePoint(platform);

  NullScenePlatformController::BucketStats stats =
      platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  (void)stats;
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
    requestChildPump(scene, platform);
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
  (void)drainSequence;
  assert(drainSequence != 0);
  (void)windowSequence;
  assert(windowSequence > drainSequence);
  assertDisposalsAreInsideSafePoints(platform);
  scene.unmount();
  g_toggleVisible = 0;
}

void testNullPlatformContract_D4_safePointDrainPrecedesWindowDrain()
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
    retireProjectedContextsWithoutApply(scene, platform);
    assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
    synchronizeThenDrainAtSafePoint(platform);
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
  (void)drainSequence;
  assert(drainSequence != 0);
  (void)windowSequence;
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
  requestChildPump(scene, platform);
  const std::size_t eventCountBeforeReclaim = platform.eventLog().size();
  LOKA_VERIFY(!scene.flushInvalidation());
  (void)eventCountBeforeReclaim;
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
  requestChildPump(scene, platform);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED) == 1);
  assertDisposalsAreInsideSafePoints(platform);
  g_toggleVisible = 0;
  g_toggleHint = loka::app::scene::NATIVE_HINT_DEFAULT;
}

void testNullPlatformContract_E2_nativeRetirementDrainIsDisposalSafePoint()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  g_toggleHint = loka::app::scene::NATIVE_HINT_EAGER_RELEASE;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  retireProjectedContextsWithoutApply(scene, platform);
  assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
  synchronizeThenDrainAtSafePoint(platform);
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
  LOKA_VERIFY(!scene.flushInvalidation());
  (void)eventsBeforeDrain;
  assert(platform.eventLog().size() == eventsBeforeDrain &&
         "reclamation is silent — no fact reaches a context from the drain");
  assert(platform.retiredCount() == 0);

  visible.set(false);
  requestChildPump(scene, platform);
  assert(platform.ledger().empty() &&
         "every native pair is handed over at a retire door, not parked past it");
  assert(platform.retiredCount() == 0);

  eventsBeforeDrain = platform.eventLog().size();
  LOKA_VERIFY(!scene.flushInvalidation());
  assert(platform.eventLog().size() == eventsBeforeDrain &&
         "the final drain is silent too");
  assert(platform.retiredCount() == 0);
  scene.unmount();
  assert(platform.createdCount() == platform.disposedCount() + platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth &&
         "teardown closes every pair: disposed or pooled, nothing lost");
  g_parkedSubtreeVisible = 0;
  g_parkedInnerCondition = 0;
}

void testNullPlatformContract_E3_safePointSettlesRetireDoorIntake()
{
  loka::core::MutableState<bool> visible(true);
  loka::core::MutableState<bool> inner(true);
  g_parkedSubtreeVisible = &visible;
  g_parkedInnerCondition = &inner;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ParkedBranchRetireBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 1);

  inner.set(false);
  retireProjectedContextsWithoutApply(scene, platform);
  assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
  synchronizeThenDrainAtSafePoint(platform);
  std::size_t eventsBeforeDrain = platform.eventLog().size();
  (void)eventsBeforeDrain;
  LOKA_VERIFY(!scene.flushInvalidation());
  assert(platform.eventLog().size() == eventsBeforeDrain);
  assert(platform.retiredCount() == 0);

  assert(platform.ledger().empty());
  assert(platform.retiredCount() == 0);

  eventsBeforeDrain = platform.eventLog().size();
  LOKA_VERIFY(!scene.flushInvalidation());
  assert(platform.eventLog().size() == eventsBeforeDrain);
  assert(platform.retiredCount() == 0);
  scene.unmount();
  assert(platform.createdCount() ==
         platform.disposedCount() +
             platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth);
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
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(probeRecord);
  const int constructionsBefore = probeRecord.constructionCount;
  const std::size_t transitionsBefore = probeRecord.transitions.size();
  const NativeContextCallCounts callsBefore(platform);

  unrelated.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  const NativeContextCallCounts callsAfter(platform);
  const bool parkedProbeSurvived =
      probeRecord.constructionCount == constructionsBefore &&
      !recordedTransitionTo(probeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            transitionsBefore);
  const bool nativeContextCallsStayedEqual = callsAfter == callsBefore;

  (void)parkedProbeSurvived;
  (void)nativeContextCallsStayedEqual;
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
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(probeRecord);
  const int constructionsBefore = probeRecord.constructionCount;
  const std::size_t transitionsBefore = probeRecord.transitions.size();
  const NativeContextCallCounts callsBefore(platform);

  unrelated.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  const NativeContextCallCounts callsAfter(platform);
  const bool parkedDraftSurvived =
      probeRecord.constructionCount == constructionsBefore &&
      !recordedTransitionTo(probeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            transitionsBefore) &&
      callsAfter == callsBefore &&
      draft.get().equals(loka::core::String::Literal("unfinished draft"));

  (void)parkedDraftSurvived;
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
  LOKA_VERIFY(scene.flushInvalidation());
  assert(NativeContextCallCounts(platform) == callsBefore &&
         "an unrelated recompose retains the Conditional seat and its native pair");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
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

void testNullPlatformContract_H3_conditionFlipIsProjectedAtNextScheduledApply()
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

  (void)probeFactsStayedUnchanged;
  (void)nativeContextCallsStayedEqual;
  (void)nativeEventLogStayedEqual;
  (void)nativeLedgerStayedEqual;
  assert(probeFactsStayedUnchanged &&
         nativeContextCallsStayedEqual &&
         nativeEventLogStayedEqual &&
         nativeLedgerStayedEqual &&
         "a condition flip remains unobservable until the next scheduled apply");

  LOKA_VERIFY(scene.flushInvalidation());
  const NullScenePlatformController::LedgerRow *button =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const NullScenePlatformController::LedgerRow *editText =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  (void)button;
  assert(button && !button->visible);
  (void)editText;
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
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(secondRecord);

  firstCondition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
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
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(probeRecord);
  const int constructionsBefore = probeRecord.constructionCount;
  const std::size_t transitionsBefore = probeRecord.transitions.size();
  const NativeContextCallCounts callsBefore(platform);

  unrelated.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  const bool taggedSeatSurvived =
      probeRecord.constructionCount == constructionsBefore &&
      !recordedTransitionTo(probeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            transitionsBefore) &&
      NativeContextCallCounts(platform) == callsBefore;
  (void)taggedSeatSurvived;
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
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(parkedRecord);
  const int constructionsBefore = parkedRecord.constructionCount;
  const std::size_t transitionsBefore = parkedRecord.transitions.size();

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

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
  (void)seatRetained;
  assert(seatRetained &&
         "the Conditional seat remains present across active-branch content recompose");

  (void)contentFresh;
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
  (void)parkedBefore;
  assert(parkedBefore && parkedBefore->visible);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(parkedRecord);
  const int activeConstructionsBefore = activeRecord.constructionCount;
  const std::size_t activeTransitionsBefore = activeRecord.transitions.size();

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  const bool seatRetainedThroughRecompose =
      activeRecord.constructionCount == activeConstructionsBefore &&
      !recordedTransitionTo(activeRecord,
                            loka::app::scene::NODE_FACT_RETIRED,
                            activeTransitionsBefore);
  assert(seatRetainedThroughRecompose &&
         "the Conditional seat remains present across parked-branch content recompose");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

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
  (void)seatRetained;
  assert(seatRetained &&
         "the Conditional seat remains present through branch re-entry");

  (void)contentFresh;
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
  LOKA_VERIFY(scene.flushInvalidation());

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  outerCondition.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  innerCondition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

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
  LOKA_VERIFY(scene.flushInvalidation());
  assert(oldSourceRecord.constructionCount == 1);
  loka::app::scene::Node *branchBeforeHide = currentSourceRecord.node;
  (void)branchBeforeHide;
  assert(branchBeforeHide &&
         "Show exposes its active branch before the ledger round-trip");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(oldSourceRecord);

  revision.set(2);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

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
  (void)control;
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
  LOKA_VERIFY(scene.flushInvalidation());

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  outerCondition.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  innerCondition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

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
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(record);

  useReplacement = true;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
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
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(directOldRoot);
  assertParkedTransitionTable(nestedOldRoot);
  assertParkedTransitionTable(failedApplyOldRoot);
  assertParkedTransitionTable(removedOldRoot);

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

  visible.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());

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
  requestChildPump(scene, platform);
  inputs.condition = &currentCondition;
  requestChildPump(scene, platform);

  currentCondition.set(false);
  if (scene.hasPendingInvalidation())
  {
    LOKA_VERIFY(scene.flushInvalidation());
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
  requestChildPump(scene, platform);

  (void)parkedConstructionsBefore;
  (void)parkedTransitionsBefore;
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
    LOKA_VERIFY(scene.flushInvalidation());
  }
  const NullScenePlatformController::LedgerRow *button =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const NullScenePlatformController::LedgerRow *editText =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(activeRecord.constructionCount == 1);
  assertParkedTransitionTable(parkedRecord);
  (void)button;
  assert(button && button->visible);
  (void)editText;
  assert(editText && !editText->visible);

  const int activeConstructionsBefore = activeRecord.constructionCount;
  const std::size_t activeTransitionsBefore = activeRecord.transitions.size();
  const NativeContextCallCounts callsBeforeSecondFlip(platform);
  replacementCondition.set(false);
  if (scene.hasPendingInvalidation())
  {
    (void)activeConstructionsBefore;
    (void)activeTransitionsBefore;
    assert(activeRecord.constructionCount == activeConstructionsBefore &&
           activeRecord.transitions.size() == activeTransitionsBefore &&
           NativeContextCallCounts(platform) == callsBeforeSecondFlip);
    LOKA_VERIFY(scene.flushInvalidation());
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
  requestChildPump(scene, platform);
  assert(platform.retiredCount() == 0);
  g_toggleVisible = 0;
}

void testNullPlatformContract_F1_safePointEmptiesRetiredQueue()
{
  loka::core::MutableState<bool> visible(true);
  g_toggleVisible = &visible;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  retireProjectedContextsWithoutApply(scene, platform);
  assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
  synchronizeThenDrainAtSafePoint(platform);
  assert(platform.retiredCount() == 0);
  g_toggleVisible = 0;
}

void testNullPlatformContract_F2_createdHandlesAreDisposedAtTeardown()
{
  loka::app::FragmentDefinition controls;
  controls << loka::app::Button("button") << loka::app::EditText();
  NullScenePlatformController platform;
  loka::app::scene::NodeDefinitionBase *rootDefinition = controls.clone();
  LOKA_VERIFY(rootDefinition != 0);
  loka::app::scene::Scene scene(rootDefinition);
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
  requestChildPump(scene, platform);

  assert(platform.ledger().empty());
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_CREATED) == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN) == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_HIDDEN) == 0);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_DISPOSED) == 0);
  assert(platform.teardownCounters().hitRouteRemoved == 0);
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
    loka::app::scene::NodeDefinitionBase *rootDefinition = button.clone();
    LOKA_VERIFY(rootDefinition != 0);
    props.scene(new loka::app::scene::Scene(rootDefinition));
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
    loka::app::scene::NodeDefinitionBase *rootDefinition = button.clone();
    LOKA_VERIFY(rootDefinition != 0);
    props.scene(new loka::app::scene::Scene(rootDefinition));
    Window *window = platformContext.createWindow(props);
    LOKA_VERIFY(window->scene());
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
  LOKA_VERIFY(scene.flushInvalidation());

  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].visible);
  g_toggleVisible = 0;
}

namespace
{
  int g_bankedSectionBank = 0;

  class BankedSectionBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<BankedSectionBoundaryNode>
      BankedSectionBoundaryProps;

  /** MineSweeper's New Game shape reduced to the mechanism: a boundary whose
      declaration swaps every Section value key on recompose (identity change,
      #277). The controls live inside the sections; a fresh bank must present
      fresh native controls. */
  class BankedSectionBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<BankedSectionBoundaryNode,
                                                         BankedSectionBoundaryProps>
  {
  public:
    explicit BankedSectionBoundaryNode(const BankedSectionBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<BankedSectionBoundaryNode,
                                                    BankedSectionBoundaryProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      for (int i = 0; i < 2; ++i)
      {
        loka::app::Section section(static_cast<loka::app::scene::NodeTag>(
            9000 + g_bankedSectionBank * 2 + i));
        loka::app::ButtonDefinition cell("cell");
        section << cell;
        root << section;
      }
      composition.declare(root);
    }
  };
} // namespace

void testStructureReportDoesNotStickOnDirectRoot()
{
  loka::core::MutableState<bool> visible(false);
  g_toggleVisible = &visible;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<ToggleControlBoundaryNode>()));
  mountAndAttach(scene, platform);

  // Structural cycle: the toggle materializes a control, the self-report
  // escalates it to the platform layout pass.
  visible.set(true);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
  assert(platform.ledger().size() == 1);

  // A direct-root boundary keeps its phase results across UPDATE cycles,
  // so the structure report must be reset per cycle there specifically: a
  // sticky report would deliver every later paint-only update as a
  // child-grade change and defeat the skip forever (#279 review).
  const unsigned long afterStructural = platform.onChangeCallCount();
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
  scene.flushInvalidation();
  LOKA_VERIFY(platform.onChangeCallCount() == afterStructural &&
              "a paint-only cycle on a direct root must not inherit the previous cycle's structure report");
  g_toggleVisible = 0;
}

void testBankedSectionSwapPresentsFreshControls()
{
  g_bankedSectionBank = 0;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<BankedSectionBoundaryNode>()));
  mountAndAttach(scene, platform);

  size_t visibleAfterMount = 0;
  for (size_t i = 0; i < platform.ledger().size(); ++i)
  {
    if (platform.ledger()[i].visible)
    {
      ++visibleAfterMount;
    }
  }
  assert(visibleAfterMount == 2 &&
         "both section-hosted controls must be visible after mount");

  // The identity change: every Section key swaps, the plan retires the old
  // boxes and materializes fresh ones. The platform must end up presenting
  // exactly the fresh controls.
  g_bankedSectionBank = 1;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
  for (int i = 0; scene.hasPendingInvalidation() && i < 4; ++i)
  {
    scene.flushInvalidation();
  }

  size_t visibleAfterSwap = 0;
  for (size_t i = 0; i < platform.ledger().size(); ++i)
  {
    if (platform.ledger()[i].visible)
    {
      ++visibleAfterSwap;
    }
  }
  if (visibleAfterSwap != 2)
  {
    std::fprintf(stderr,
                 "banked section swap: %lu visible controls after swap "
                 "(expected 2); ledger rows %lu, retired %lu\n",
                 static_cast<unsigned long>(visibleAfterSwap),
                 static_cast<unsigned long>(platform.ledger().size()),
                 static_cast<unsigned long>(platform.retiredCount()));
    std::abort();
  }
}

namespace
{
  /** Host-side paint record for Toolbox's retained dirty replay. A dirty
      present can repaint only contexts registered by the preceding full tree
      walk; a full present replaces that record from the current projection. */
  class ToolboxPresentPaintRecord : public NullScenePlatformController
  {
  public:
    ToolboxPresentPaintRecord()
        : fullRequested_(true),
          dirtyRequested_(false),
          flushFullCount_(0),
          flushDirtyCount_(0),
          paintedHandleIds_()
    {
    }

    void requestDirty()
    {
      if (!this->fullRequested_)
      {
        this->dirtyRequested_ = true;
      }
    }

    virtual void releaseNodeContexts(loka::app::scene::Node *node)
    {
      if (node)
      {
        this->fullRequested_ = true;
        this->dirtyRequested_ = false;
      }
      NullScenePlatformController::releaseNodeContexts(node);
    }

    void present()
    {
      if (this->fullRequested_)
      {
        ++this->flushFullCount_;
        this->paintedHandleIds_.clear();
        for (size_t i = 0; i < this->ledger().size(); ++i)
        {
          const LedgerRow &row = this->ledger()[i];
          if (row.visible && row.handle)
          {
            this->paintedHandleIds_.push_back(row.handle->id);
          }
        }
      }
      else if (this->dirtyRequested_)
      {
        ++this->flushDirtyCount_;
      }
      this->fullRequested_ = false;
      this->dirtyRequested_ = false;
    }

    void resetFlushCounts()
    {
      this->flushFullCount_ = 0;
      this->flushDirtyCount_ = 0;
    }

    bool freshlyMaterializedChildrenPainted() const
    {
      for (size_t i = 0; i < this->ledger().size(); ++i)
      {
        const LedgerRow &row = this->ledger()[i];
        if (!row.visible || !row.handle)
        {
          continue;
        }
        bool found = false;
        for (size_t j = 0; j < this->paintedHandleIds_.size(); ++j)
        {
          if (this->paintedHandleIds_[j] == row.handle->id)
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
          return false;
        }
      }
      return true;
    }

    int flushFullCount() const
    {
      return this->flushFullCount_;
    }

    int flushDirtyCount() const
    {
      return this->flushDirtyCount_;
    }

  private:
    bool fullRequested_;
    bool dirtyRequested_;
    int flushFullCount_;
    int flushDirtyCount_;
    std::vector<int> paintedHandleIds_;
  };
} // namespace

void testToolboxPresentPointPaintsFreshBankedSectionChildren()
{
  g_bankedSectionBank = 0;
  ToolboxPresentPaintRecord platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<BankedSectionBoundaryNode>()));
  mountAndAttach(scene, platform);
  platform.present();
  LOKA_VERIFY(platform.freshlyMaterializedChildrenPainted());

  g_bankedSectionBank = 1;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
  for (int i = 0; scene.hasPendingInvalidation() && i < 4; ++i)
  {
    scene.flushInvalidation();
  }

  // Per-cell dirties are recorded during the swap episode. Retirement must
  // override them with a tree-walk present, replacing the retired paint
  // record with the freshly materialized bank.
  platform.requestDirty();
  platform.present();

  LOKA_VERIFY(platform.freshlyMaterializedChildrenPainted());
  printf("==== [testToolboxPresentPointPaintsFreshBankedSectionChildren] PASSED ====\n");
}

void testToolboxStructureSwapCollapsesToOneFullPresent()
{
  g_bankedSectionBank = 0;
  ToolboxPresentPaintRecord platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<BankedSectionBoundaryNode>()));
  mountAndAttach(scene, platform);
  platform.present();
  platform.resetFlushCounts();

  g_bankedSectionBank = 1;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
  for (int i = 0; scene.hasPendingInvalidation() && i < 4; ++i)
  {
    scene.flushInvalidation();
  }
  platform.requestDirty();
  platform.present();

  LOKA_VERIFY(platform.flushFullCount() == 1);
  LOKA_VERIFY(platform.flushDirtyCount() == 0);
  printf("==== [testToolboxStructureSwapCollapsesToOneFullPresent] PASSED ====\n");
}

void testToolboxPlainContentUpdateUsesDirtyPresent()
{
  g_bankedSectionBank = 0;
  ToolboxPresentPaintRecord platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<BankedSectionBoundaryNode>()));
  mountAndAttach(scene, platform);
  platform.present();
  platform.resetFlushCounts();

  // A content-only update does not retire or create a context. Its ordinary
  // dirty request therefore stays on the retained replay path.
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
  scene.flushInvalidation();
  platform.requestDirty();
  platform.present();

  LOKA_VERIFY(platform.flushFullCount() == 0);
  LOKA_VERIFY(platform.flushDirtyCount() == 1);
  printf("==== [testToolboxPlainContentUpdateUsesDirtyPresent] PASSED ====\n");
}

namespace
{
  struct BankedClickTypeTag
  {
  };
  class BankedClickBoundaryNode;
  struct BankedClickProps
      : public loka::app::scene::NodePropsBase<BankedClickProps>
  {
    typedef BankedClickTypeTag TypeTag;
    typedef BankedClickBoundaryNode NodeType;
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return this->propsTypeId() < rhs.propsTypeId();
    }
  };

  BankedClickBoundaryNode *g_bankedClickNode = 0;

  /** The full MineSweeper New Game drive: a compose-once StdComposition
      boundary that re-declares only on NODE_DIRTY_CHILD, flips its Section
      key bank inside a button click handler, and marks itself dirty from
      there (markViewDirty flushes synchronously mid-dispatch). */
  class BankedClickBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<BankedClickProps>
  {
  public:
    explicit BankedClickBoundaryNode(const BankedClickProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<BankedClickProps>(props),
          bank_(0),
          newGameClicks_(0)
    {
      g_bankedClickNode = this;
    }
    virtual ~BankedClickBoundaryNode()
    {
      if (g_bankedClickNode == this)
      {
        g_bankedClickNode = 0;
      }
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      this->bindUi();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      loka::app::ButtonDefinition game("game", &this->newGameClick_);
      root << game;
      loka::app::FragmentDefinition bankedCells;
      for (int i = 0; i < 2; ++i)
      {
        loka::app::Section section(static_cast<loka::app::scene::NodeTag>(
            9100 + this->bank_ * 2 + i));
        loka::app::ButtonDefinition cell("cell");
        section << cell;
        bankedCells << section;
      }
      root << bankedCells;
      composition.declare(root);
    }

    void startNewGame()
    {
      ++this->newGameClicks_;
      this->bank_ = 1 - this->bank_;
      this->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
    }

    int newGameClicks() const
    {
      return this->newGameClicks_;
    }

    loka::core::EmitterState &newGameClick()
    {
      return this->newGameClick_;
    }

  protected:
    virtual void declareLocalRecomposition(loka::app::scene::NodeComposition &composition)
    {
      this->composeNode(composition);
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::StdCompositionBoundaryNodeBase<BankedClickProps> BaseType;
      if (event == loka::app::scene::COMPOSE_EVENT_UPDATE &&
          (context.dirtyFlags() & loka::app::scene::NODE_DIRTY_CHILD))
      {
        this->recomposeLocalComposition(context, event,
                                        this->LOCAL_RECOMPOSE_APPLY_SNAPSHOT);
        this->bindUi();
        return;
      }
      BaseType::composeWithContext(context, event);
    }

  private:
    void bindUi()
    {
      this->bindForUi(this->newGameClick_, this, &BankedClickBoundaryNode::startNewGame);
    }

    int bank_;
    int newGameClicks_;
    loka::core::EmitterState newGameClick_;
  };
} // namespace

void testBankedSectionClickHandlerSwapPresentsFreshControls()
{
  NullScenePlatformController platform;
  loka::app::scene::NodeDefinition<BankedClickProps, BankedClickBoundaryNode>
      mainDefinition;
  loka::app::scene::NodeDefinitionBase *rootDefinition = mainDefinition.clone();
  LOKA_VERIFY(rootDefinition != 0);
  loka::app::scene::Scene scene(rootDefinition);
  mountAndAttach(scene, platform);
  BankedClickBoundaryNode *board = g_bankedClickNode;
  assert(board);

  size_t visibleAfterMount = 0;
  for (size_t i = 0; i < platform.ledger().size(); ++i)
  {
    if (platform.ledger()[i].visible)
    {
      ++visibleAfterMount;
    }
  }
  assert(visibleAfterMount == 3 &&
         "the game button and both section-hosted cells must be visible");

  // Drive New Game exactly the way the app does: the emitter fires from a
  // click, the handler flips the bank and marks the view dirty, and the
  // flush happens synchronously inside the dispatch.
  {
    loka::core::StateTrackerGuard guard(board->tracker());
    board->newGameClick().emit();
  }
  LOKA_VERIFY(board->newGameClicks() == 1);
  for (int i = 0; scene.hasPendingInvalidation() && i < 4; ++i)
  {
    scene.flushInvalidation();
  }

  size_t visibleAfterSwap = 0;
  for (size_t i = 0; i < platform.ledger().size(); ++i)
  {
    if (platform.ledger()[i].visible)
    {
      ++visibleAfterSwap;
    }
  }
  if (visibleAfterSwap != 3)
  {
    std::fprintf(stderr,
                 "click-driven bank swap: %lu visible controls after swap "
                 "(expected 3); ledger rows %lu, retired %lu\n",
                 static_cast<unsigned long>(visibleAfterSwap),
                 static_cast<unsigned long>(platform.ledger().size()),
                 static_cast<unsigned long>(platform.retiredCount()));
    std::abort();
  }

  // The second click is the Toolbox/Win32 kill scenario: the rebound handler
  // must fire and the world must still present three controls.
  {
    loka::core::StateTrackerGuard guard(board->tracker());
    board->newGameClick().emit();
  }
  LOKA_VERIFY(board->newGameClicks() == 2);
  for (int i = 0; scene.hasPendingInvalidation() && i < 4; ++i)
  {
    scene.flushInvalidation();
  }

  // The structure report is a per-cycle fact: after the structural cycles
  // above, a paint-only cycle must still ride the skip -- a sticky report
  // would escalate every later update through layout/ensure (#279 review).
  const unsigned long onChangeAfterSwaps = platform.onChangeCallCount();
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
  scene.flushInvalidation();
  LOKA_VERIFY(platform.onChangeCallCount() == onChangeAfterSwaps &&
              "a paint-only cycle after a structural one must not escalate");
}

namespace
{
  enum HandlerSwapControlKind
  {
    HANDLER_SWAP_BUTTON,
    HANDLER_SWAP_POPUP_MENU
  };

  HandlerSwapControlKind g_handlerSwapControlKind = HANDLER_SWAP_BUTTON;
  loka::core::State<loka::core::String> *g_handlerSwapButtonText = 0;
  loka::core::EmitterState *g_handlerSwapEmitter = 0;

  class HandlerSwapBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HandlerSwapBoundaryNode>
      HandlerSwapBoundaryProps;
  typedef SceneTestSupport::RecomposingBoundaryNode<HandlerSwapBoundaryNode,
                                                     HandlerSwapBoundaryProps,
                                                     true>
      HandlerSwapBoundaryBase;

  class HandlerSwapBoundaryNode
      : public HandlerSwapBoundaryBase
  {
  public:
    explicit HandlerSwapBoundaryNode(const HandlerSwapBoundaryProps &props)
        : HandlerSwapBoundaryBase(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_handlerSwapControlKind == HANDLER_SWAP_BUTTON)
      {
        root << loka::app::ButtonDefinition(g_handlerSwapButtonText,
                                             g_handlerSwapEmitter);
      }
      else
      {
        const char *items[] = {"first", "second"};
        loka::app::PopupMenuDefinition popup(items, 2);
        popup.onChange(g_handlerSwapEmitter);
        root << popup;
      }
      composition.declare(root);
    }
  };

  struct HandlerSwapWitness
  {
    HandlerSwapWitness()
        : calls(0)
    {
    }

    int calls;

    static void Thunk(void *userData)
    {
      HandlerSwapWitness *self = static_cast<HandlerSwapWitness *>(userData);
      if (self)
      {
        ++self->calls;
      }
    }
  };

  loka::app::scene::Node *findHandlerSwapControl(
      loka::app::scene::Node *node,
      HandlerSwapControlKind kind)
  {
    if (!node)
    {
      return 0;
    }
    if ((kind == HANDLER_SWAP_BUTTON && node->asButtonNode()) ||
        (kind == HANDLER_SWAP_POPUP_MENU && node->asPopupMenuNode()))
    {
      return node;
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
         child; child = child->nextInComposition)
    {
      loka::app::scene::Node *match = findHandlerSwapControl(child, kind);
      if (match)
      {
        return match;
      }
    }
    return 0;
  }

  void verifyHandlerOnlyRecomposeUsesCurrentEmitter(HandlerSwapControlKind kind)
  {
    loka::core::MutableState<loka::core::String> buttonText(
        loka::core::String::Literal("handler-swap"));
    loka::core::EmitterState previousEmitter;
    loka::core::EmitterState currentEmitter;
    HandlerSwapWitness previousWitness;
    HandlerSwapWitness currentWitness;
    previousEmitter.bind(&HandlerSwapWitness::Thunk, &previousWitness, false);
    currentEmitter.bind(&HandlerSwapWitness::Thunk, &currentWitness, false);
    g_handlerSwapControlKind = kind;
    g_handlerSwapButtonText = &buttonText;
    g_handlerSwapEmitter = &previousEmitter;

    {
      NullScenePlatformController platform;
      loka::app::scene::Scene scene(
          (loka::app::scene::Boundary<HandlerSwapBoundaryNode>()));
      mountAndAttach(scene, platform);

      loka::app::scene::Node *original = findHandlerSwapControl(
          loka::dsl::testing::SceneTestAccess::rootNode(scene), kind);
      (void)original;
      assert(original);

      g_handlerSwapEmitter = &currentEmitter;
      scene.requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
      LOKA_VERIFY(scene.flushInvalidation());

      loka::app::scene::Node *updated = findHandlerSwapControl(
          loka::dsl::testing::SceneTestAccess::rootNode(scene), kind);
      assert(updated == original &&
             "a handler-only recompose must retain the compatible control node");

      loka::core::EmitterState *appliedEmitter =
          kind == HANDLER_SWAP_BUTTON
              ? updated->asButtonNode()->props.onClick_
              : updated->asPopupMenuNode()->props.onChange_;
      assert(appliedEmitter == &currentEmitter &&
             "retained props must apply a handler-only identity change");
      appliedEmitter->emit();
      assert(previousWitness.calls == 0 &&
             "a retained control must stop gesturing at the previous owner");
      assert(currentWitness.calls == 1 &&
             "the recomposed handler must receive the next gesture");
    }

    previousEmitter.unbind(&HandlerSwapWitness::Thunk, &previousWitness);
    currentEmitter.unbind(&HandlerSwapWitness::Thunk, &currentWitness);
    g_handlerSwapButtonText = 0;
    g_handlerSwapEmitter = 0;
  }
} // namespace

void testButtonHandlerOnlyRecomposeUsesCurrentEmitter()
{
  verifyHandlerOnlyRecomposeUsesCurrentEmitter(HANDLER_SWAP_BUTTON);
}

void testPopupMenuHandlerOnlyRecomposeUsesCurrentEmitter()
{
  verifyHandlerOnlyRecomposeUsesCurrentEmitter(HANDLER_SWAP_POPUP_MENU);
}

namespace
{
  typedef char MatchingValueBindingSignature;
  struct MissingValueBindingSignature
  {
    char bytes[2];
  };

  template <typename PropsT, typename ValueT> struct ControlValueBindingDoorProbe
  {
    typedef PropsT &(PropsT::*MutableDoor)(loka::core::MutableState<ValueT> *);
  };

  template <typename PropsT> struct EditTextImmutableValueDoorProbe
  {
    typedef PropsT &(PropsT::*Door)(loka::core::State<loka::core::String> *);
    template <Door> struct Match
    {
    };
    template <typename Candidate> static MatchingValueBindingSignature Test(Match<&Candidate::text> *);
    template <typename Candidate> static MissingValueBindingSignature Test(...);
    enum
    {
      kAccepts = sizeof(Test<PropsT>(0)) == sizeof(MatchingValueBindingSignature)
    };
  };

  template <typename PropsT> struct PopupMenuImmutableValueDoorProbe
  {
    typedef PropsT &(PropsT::*Door)(loka::core::State<int> *);
    template <Door> struct Match
    {
    };
    template <typename Candidate> static MatchingValueBindingSignature Test(Match<&Candidate::selectedIndex> *);
    template <typename Candidate> static MissingValueBindingSignature Test(...);
    enum
    {
      kAccepts = sizeof(Test<PropsT>(0)) == sizeof(MatchingValueBindingSignature)
    };
  };

  template <typename PropsT> struct ScrollBarImmutableValueDoorProbe
  {
    typedef PropsT &(PropsT::*Door)(loka::core::State<int> *);
    template <Door> struct Match
    {
    };
    template <typename Candidate> static MatchingValueBindingSignature Test(Match<&Candidate::value> *);
    template <typename Candidate> static MissingValueBindingSignature Test(...);
    enum
    {
      kAccepts = sizeof(Test<PropsT>(0)) == sizeof(MatchingValueBindingSignature)
    };
  };

  typedef ControlValueBindingDoorProbe<loka::app::EditTextProps,
                                       loka::core::String>
      EditTextValueDoorProbe;
  typedef ControlValueBindingDoorProbe<loka::app::PopupMenuProps, int>
      PopupMenuValueDoorProbe;
  typedef ControlValueBindingDoorProbe<loka::app::ScrollBarProps, int>
      ScrollBarValueDoorProbe;
}

void testControlValueBindingsRequireMutableState()
{
  EditTextValueDoorProbe::MutableDoor editTextDoor = &loka::app::EditTextProps::text;
  PopupMenuValueDoorProbe::MutableDoor popupMenuDoor = &loka::app::PopupMenuProps::selectedIndex;
  ScrollBarValueDoorProbe::MutableDoor scrollBarDoor = &loka::app::ScrollBarProps::value;
  (void)editTextDoor;
  (void)popupMenuDoor;
  (void)scrollBarDoor;

  assert(!EditTextImmutableValueDoorProbe<loka::app::EditTextProps>::kAccepts &&
         "EditText value binding must refuse immutable State<String>");
  assert(!PopupMenuImmutableValueDoorProbe<loka::app::PopupMenuProps>::kAccepts &&
         "PopupMenu value binding must refuse immutable State<int>");
  assert(!ScrollBarImmutableValueDoorProbe<loka::app::ScrollBarProps>::kAccepts &&
         "ScrollBar value binding must refuse immutable State<int>");
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
  (void)button;
  assert(button && button->visible);
  LOKA_VERIFY(!platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT));
  assert(g_stdCompositionShowShapeNode);

  g_stdCompositionShowShapeNode->openDialog();
  if (scene.hasPendingInvalidation())
  {
    LOKA_VERIFY(scene.flushInvalidation());
  }

  button = platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON);
  const NullScenePlatformController::LedgerRow *dialog =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(button && button->visible &&
         "siblings survive a Show flip inside a compose-once boundary");
  (void)dialog;
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
  (void)button;
  assert(button && button->visible);

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  const NullScenePlatformController::LedgerRow *edit =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  (void)edit;
  assert(edit && edit->visible &&
         "root-seat rebuild attaches the selected compose-once branch");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
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
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(record);

  useReplacement = true;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedRetirementTransitionTable(record);

  useReplacement = false;
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
  condition.set(false);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());
  assert(record.constructionCount == 2 && record.node &&
         record.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "post-generation flip resolves only the recreated seat mapping");

  scene.unmount();
}

namespace
{
  class DialogPresentationProbeNode;

  struct DialogPresentationRecord
  {
    DialogPresentationRecord()
        : constructionCount(0),
          destructionCount(0),
          presentCount(0),
          sessionDisposeCount(0),
          nextInstanceId(0),
          node(0),
          transitions(),
          attachPhaseInputs()
    {
    }

    int constructionCount;
    int destructionCount;
    int presentCount;
    int sessionDisposeCount;
    int nextInstanceId;
    DialogPresentationProbeNode *node;
    std::vector<FactTransition> transitions;
    std::vector<loka::app::OpenFileDialogPresentationState> attachPhaseInputs;
  };

  bool dialogRecordedTransitionTo(
      const DialogPresentationRecord &record,
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

  class DialogPresentationProbeContext : public loka::app::scene::NodeContext
  {
  public:
    explicit DialogPresentationProbeContext(DialogPresentationRecord *record)
        : record_(record),
          presentation_()
    {
    }

    void applyInstalledFact()
    {
      if (this->owner() &&
          this->owner()->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
      {
        this->applyAttachedPresentation();
      }
    }

    virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                               loka::app::scene::NodeLifecycleFact next)
    {
      if (this->record_)
      {
        this->record_->transitions.push_back(FactTransition(previous, next));
      }
      if (next == loka::app::scene::NODE_FACT_ATTACHED)
      {
        this->applyAttachedPresentation();
        return;
      }
      this->presentation_.markDetached();
      if (this->record_)
      {
        ++this->record_->sessionDisposeCount;
      }
    }

  private:
    void applyAttachedPresentation()
    {
      if (this->record_)
      {
        this->record_->attachPhaseInputs.push_back(this->presentation_.value);
      }
      if (!this->presentation_.beginPresent())
      {
        return;
      }
      if (this->record_)
      {
        ++this->record_->presentCount;
      }
      this->presentation_.markPresented();
    }

    DialogPresentationRecord *record_;
    loka::app::OpenFileDialogPresentationPhase presentation_;
  };

  struct DialogPresentationProbeTypeTag
  {
  };

  struct DialogPresentationProbeProps
      : public loka::app::scene::NodePropsBase<DialogPresentationProbeProps>
  {
    typedef DialogPresentationProbeTypeTag TypeTag;
    typedef DialogPresentationProbeNode NodeType;

    explicit DialogPresentationProbeProps(DialogPresentationRecord *recordValue = 0)
        : record(recordValue)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const DialogPresentationProbeProps &other =
          static_cast<const DialogPresentationProbeProps &>(rhs);
      return this->record < other.record;
    }

    DialogPresentationRecord *record;
  };

  class DialogPresentationProbeNode : public loka::app::scene::ComposableNode
  {
  public:
    typedef DialogPresentationProbeTypeTag TypeTag;

    explicit DialogPresentationProbeNode(const DialogPresentationProbeProps &propsValue)
        : props(propsValue),
          branchValue_(),
          instanceId_(0)
    {
      this->state(this->branchValue_, 17);
      if (this->props.record)
      {
        ++this->props.record->constructionCount;
        this->instanceId_ = ++this->props.record->nextInstanceId;
        this->props.record->node = this;
      }
      DialogPresentationProbeContext *context =
          new DialogPresentationProbeContext(this->props.record);
      this->setContext(context);
      context->applyInstalledFact();
    }

    virtual ~DialogPresentationProbeNode()
    {
      if (this->props.record)
      {
        ++this->props.record->destructionCount;
        if (this->props.record->node == this)
        {
          this->props.record->node = 0;
        }
      }
    }

    int branchValue() const
    {
      return this->branchValue_.get();
    }

    void setBranchValue(int value)
    {
      this->branchValue_.set(value);
    }

    int instanceId() const
    {
      return this->instanceId_;
    }

    DialogPresentationProbeProps props;

  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      (void)context;
      (void)event;
    }

  private:
    loka::app::scene::NodeState<int> branchValue_;
    int instanceId_;
  };

  class DialogPresentationProbeDefinition
      : public loka::app::scene::NodeDefinition<DialogPresentationProbeProps,
                                                DialogPresentationProbeNode>
  {
  public:
    typedef loka::app::scene::NodeDefinition<DialogPresentationProbeProps,
                                              DialogPresentationProbeNode>
        BaseType;

    explicit DialogPresentationProbeDefinition(DialogPresentationRecord *record)
        : BaseType(DialogPresentationProbeProps(record))
    {
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      DialogPresentationProbeDefinition *copy =
          new DialogPresentationProbeDefinition(this->props.record);
      if (copy)
      {
        copy->copyTestIdPolicyFrom(*this);
      }
      return copy;
    }

    virtual loka::app::scene::NodeKind nodeKind() const
    {
      return loka::app::scene::NODE_KIND_UNKNOWN;
    }
  };

  loka::core::MutableState<bool> *g_dialogDefaultShown = 0;
  loka::core::MutableState<bool> *g_dialogDestroyShown = 0;
  DialogPresentationRecord *g_dialogDefaultRecord = 0;
  DialogPresentationRecord *g_dialogDestroyRecord = 0;

  void declareDialogPresentationPolicySeats(
      loka::app::scene::NodeComposition &composition,
      loka::core::State<bool> *defaultShown,
      loka::core::State<bool> *destroyShown)
  {
    DialogPresentationProbeDefinition defaultProbe(g_dialogDefaultRecord);
    loka::app::ShowDefinition defaultSeat = loka::app::Show(*defaultShown);
    defaultSeat << defaultProbe;

    DialogPresentationProbeDefinition destroyProbe(g_dialogDestroyRecord);
    loka::app::PolicyScopeDefinition destroyScope;
    destroyScope.destroyOnDetach() << destroyProbe;
    loka::app::ShowDefinition destroySeat = loka::app::Show(*destroyShown);
    destroySeat << destroyScope;

    loka::app::FragmentDefinition root;
    root << defaultSeat << destroySeat;
    composition.declare(root);
  }

  class DialogPresentationRecomposeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<DialogPresentationRecomposeBoundaryNode>
      DialogPresentationRecomposeBoundaryProps;
  class DialogPresentationRecomposeBoundaryNode
      : public PropsRecomposingBoundaryNode<DialogPresentationRecomposeBoundaryNode,
                                            DialogPresentationRecomposeBoundaryProps>
  {
  public:
    explicit DialogPresentationRecomposeBoundaryNode(
        const DialogPresentationRecomposeBoundaryProps &props)
        : PropsRecomposingBoundaryNode<DialogPresentationRecomposeBoundaryNode,
                                       DialogPresentationRecomposeBoundaryProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      declareDialogPresentationPolicySeats(composition,
                                           g_dialogDefaultShown,
                                           g_dialogDestroyShown);
    }
  };

  class DialogPresentationRecomposeHarnessNode;
  typedef loka::app::scene::BoundaryPropsFor<DialogPresentationRecomposeHarnessNode>
      DialogPresentationRecomposeHarnessProps;
  class DialogPresentationRecomposeHarnessNode
      : public loka::app::scene::BoundaryNodeFor<DialogPresentationRecomposeHarnessNode>
  {
  public:
    explicit DialogPresentationRecomposeHarnessNode(
        const DialogPresentationRecomposeHarnessProps &props)
        : loka::app::scene::BoundaryNodeFor<DialogPresentationRecomposeHarnessNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(
          loka::app::scene::Boundary<DialogPresentationRecomposeBoundaryNode>());
    }
  };

  class DialogPresentationComposeOnceBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<DialogPresentationComposeOnceBoundaryNode>
      DialogPresentationComposeOnceBoundaryProps;
  DialogPresentationComposeOnceBoundaryNode *g_dialogComposeOnceNode = 0;
  class DialogPresentationComposeOnceBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<
            DialogPresentationComposeOnceBoundaryProps>
  {
  public:
    explicit DialogPresentationComposeOnceBoundaryNode(
        const DialogPresentationComposeOnceBoundaryProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<
              DialogPresentationComposeOnceBoundaryProps>(props),
          defaultShown_(),
          destroyShown_()
    {
      this->state(this->defaultShown_, false);
      this->state(this->destroyShown_, false);
      g_dialogComposeOnceNode = this;
    }

    virtual ~DialogPresentationComposeOnceBoundaryNode()
    {
      if (g_dialogComposeOnceNode == this)
      {
        g_dialogComposeOnceNode = 0;
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      declareDialogPresentationPolicySeats(composition,
                                           this->defaultShown_.state(),
                                           this->destroyShown_.state());
    }

    void showBoth()
    {
      this->defaultShown_.set(true, true);
      this->destroyShown_.set(true, true);
    }

    void hideBoth()
    {
      this->defaultShown_.set(false, true);
      this->destroyShown_.set(false, true);
    }

  private:
    loka::app::scene::NodeState<bool> defaultShown_;
    loka::app::scene::NodeState<bool> destroyShown_;
  };

  void assertDialogPresentationPolicyInitialShow(
      const DialogPresentationRecord &defaultRecord,
      const DialogPresentationRecord &destroyRecord)
  {
    (void)defaultRecord;
    assert(defaultRecord.constructionCount == 1 &&
           defaultRecord.destructionCount == 0 &&
           defaultRecord.presentCount == 1 &&
           defaultRecord.sessionDisposeCount == 0 &&
           defaultRecord.node &&
           defaultRecord.node->branchValue() == 17 &&
           "default Show constructs and presents one retained dialog instance");
    (void)destroyRecord;
    assert(destroyRecord.constructionCount == 1 &&
           destroyRecord.destructionCount == 0 &&
           destroyRecord.presentCount == 1 &&
           destroyRecord.sessionDisposeCount == 0 &&
           destroyRecord.node &&
           destroyRecord.node->branchValue() == 17 &&
           "destroyOnDetach Show constructs and presents one dialog instance");
    assert(defaultRecord.attachPhaseInputs.size() == 1 &&
           defaultRecord.attachPhaseInputs[0] ==
               loka::app::OPEN_FILE_DIALOG_PRESENTATION_PENDING_ATTACH &&
           destroyRecord.attachPhaseInputs.size() == 1 &&
           destroyRecord.attachPhaseInputs[0] ==
               loka::app::OPEN_FILE_DIALOG_PRESENTATION_PENDING_ATTACH &&
           "new dialog contexts begin from the real pending-attach phase");
  }

  void assertDialogPresentationPolicyDetached(
      const DialogPresentationRecord &defaultRecord,
      const DialogPresentationRecord &destroyRecord,
      DialogPresentationProbeNode *defaultInstance)
  {
    (void)&dialogRecordedTransitionTo;
    (void)defaultRecord;
    (void)defaultInstance;
    assert(defaultRecord.sessionDisposeCount == 1 &&
           defaultRecord.destructionCount == 0 &&
           defaultRecord.node == defaultInstance &&
           defaultRecord.node->branchValue() == 73 &&
           dialogRecordedTransitionTo(defaultRecord,
                                      loka::app::scene::NODE_FACT_DETACHED_RETAINED,
                                      0) &&
           "default Show disposes its native session but retains node, context, and state");
    (void)destroyRecord;
    assert(destroyRecord.sessionDisposeCount == 1 &&
           dialogRecordedTransitionTo(destroyRecord,
                                      loka::app::scene::NODE_FACT_RETIRED,
                                      0) &&
           "destroyOnDetach disposes its session at the synchronous retire door");
  }

  void assertDialogPresentationPolicyReentered(
      const DialogPresentationRecord &defaultRecord,
      const DialogPresentationRecord &destroyRecord,
      DialogPresentationProbeNode *defaultInstance,
      int defaultInstanceId,
      int destroyInstanceId)
  {
    (void)defaultRecord;
    (void)defaultInstance;
    (void)defaultInstanceId;
    assert(defaultRecord.constructionCount == 1 &&
           defaultRecord.destructionCount == 0 &&
           defaultRecord.presentCount == 2 &&
           defaultRecord.sessionDisposeCount == 1 &&
           defaultRecord.node == defaultInstance &&
           defaultRecord.node->instanceId() == defaultInstanceId &&
           defaultRecord.node->branchValue() == 73 &&
           dialogRecordedTransitionTo(defaultRecord,
                                      loka::app::scene::NODE_FACT_ATTACHED,
                                      1) &&
           "default Show reuses its retained instance and branch-local state");
    (void)destroyRecord;
    (void)destroyInstanceId;
    assert(destroyRecord.constructionCount == 2 &&
           destroyRecord.destructionCount == 1 &&
           destroyRecord.presentCount == 2 &&
           destroyRecord.sessionDisposeCount == 1 &&
           destroyRecord.node &&
           destroyRecord.node->instanceId() != destroyInstanceId &&
           destroyRecord.node->branchValue() == 17 &&
           "destroyOnDetach Show creates a fresh instance with fresh branch-local state");
    assert(defaultRecord.attachPhaseInputs.size() == 2 &&
           defaultRecord.attachPhaseInputs[1] ==
               loka::app::OPEN_FILE_DIALOG_PRESENTATION_PENDING_ATTACH &&
           destroyRecord.attachPhaseInputs.size() == 2 &&
           destroyRecord.attachPhaseInputs[1] ==
               loka::app::OPEN_FILE_DIALOG_PRESENTATION_PENDING_ATTACH &&
           "retained and reconstructed dialog phases both re-arm before presentation");
  }

  void drainDialogRetirement(loka::app::scene::Scene &scene)
  {
    int remainingRuns = 8;
    while (scene.hasPendingInvalidation() && remainingRuns-- > 0)
    {
      scene.flushInvalidation();
    }
    assert(!scene.hasPendingInvalidation() &&
           "dialog retirement reclaim settles at a later tracker boundary");
  }

  void clearDialogPresentationGlobals()
  {
    g_dialogDefaultShown = 0;
    g_dialogDestroyShown = 0;
    g_dialogDefaultRecord = 0;
    g_dialogDestroyRecord = 0;
  }

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
      loka::app::PolicyScopeDefinition scope;
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

  class PolicyMisplacedBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyMisplacedBoundaryNode>
      PolicyMisplacedBoundaryProps;
  class PolicyMisplacedBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<PolicyMisplacedBoundaryNode>
  {
  public:
    explicit PolicyMisplacedBoundaryNode(const PolicyMisplacedBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<PolicyMisplacedBoundaryNode>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      ParkedFactDefinition content((ParkedFactProps(g_policyDefaultFact)));
      loka::app::PolicyScopeDefinition scope;
      scope.destroyOnDetach() << content;
      loka::app::FragmentDefinition misplacedBranch;
      misplacedBranch << scope;
      loka::app::FragmentDefinition hidden;
      loka::app::scene::ConditionalDefinition seat(
          (loka::app::scene::ConditionalProps(g_policyDefaultCondition,
                                              &misplacedBranch,
                                              &hidden)));
      loka::app::FragmentDefinition root;
      root << seat;
      composition.declare(root);
    }
  };

  class PolicyMisplacedRecomposeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyMisplacedRecomposeBoundaryNode>
      PolicyMisplacedRecomposeBoundaryProps;
  class PolicyMisplacedRecomposeBoundaryNode
      : public PropsRecomposingBoundaryNode<PolicyMisplacedRecomposeBoundaryNode,
                                            PolicyMisplacedRecomposeBoundaryProps>
  {
  public:
    explicit PolicyMisplacedRecomposeBoundaryNode(
        const PolicyMisplacedRecomposeBoundaryProps &props)
        : PropsRecomposingBoundaryNode<PolicyMisplacedRecomposeBoundaryNode,
                                       PolicyMisplacedRecomposeBoundaryProps>(props)
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
        registrar.markDirtyOnChange(g_policyRevision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      ParkedFactDefinition content((ParkedFactProps(g_policyDefaultFact)));
      loka::app::PolicyScopeDefinition misplacedScope;
      misplacedScope.destroyOnDetach() << content;
      loka::app::FragmentDefinition root;
      root << misplacedScope;
      composition.declare(root);
    }
  };

  class PolicyMisplacedReplacementBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PolicyMisplacedReplacementBoundaryNode>
      PolicyMisplacedReplacementBoundaryProps;
  class PolicyMisplacedReplacementBoundaryNode
      : public PropsRecomposingBoundaryNode<PolicyMisplacedReplacementBoundaryNode,
                                            PolicyMisplacedReplacementBoundaryProps>
  {
  public:
    explicit PolicyMisplacedReplacementBoundaryNode(
        const PolicyMisplacedReplacementBoundaryProps &props)
        : PropsRecomposingBoundaryNode<PolicyMisplacedReplacementBoundaryNode,
                                       PolicyMisplacedReplacementBoundaryProps>(props)
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
        registrar.markDirtyOnChange(g_policyRevision,
                                    loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      const int revision = g_policyRevision ? g_policyRevision->get() : 0;
      if (revision == 0)
      {
        loka::app::ButtonDefinition button("plain-fragment-button");
        loka::app::FragmentDefinition plainFragment;
        plainFragment << button;
        root << plainFragment;
      }
      else if (revision == 2)
      {
        loka::app::ButtonDefinition button("direct-button");
        root << button;
      }
      else
      {
        loka::app::EditTextDefinition edit;
        loka::app::PolicyScopeDefinition misplacedScope;
        misplacedScope.destroyOnDetach() << edit;
        root << misplacedScope;
      }
      composition.declare(root);
    }
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
  (void)content;
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

void testPolicyScopeHandlesNonBranchRootPlacementGracefully()
{
  loka::core::MutableState<bool> condition(true);
  ParkedFactRecord contentRecord;
  g_policyDefaultCondition = &condition;
  g_policyDefaultFact = &contentRecord;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<PolicyMisplacedBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(contentRecord.constructionCount == 1 && contentRecord.node &&
         "a misplaced PolicyScope preserves its inner content");

  {
    loka::core::StateTrackerGuard guard(condition.trackerOwner());
    condition.set(false);
  }
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  assertParkedTransitionTable(contentRecord);

  {
    loka::core::StateTrackerGuard guard(condition.trackerOwner());
    condition.set(true);
  }
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  assert(contentRecord.constructionCount == 1 &&
         contentRecord.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "a misplaced PolicyScope ignores destroyOnDetach and uses default parking");
  scene.unmount();
  clearPolicyGlobals();
}

void testMisplacedPolicyScopeRetainsInnerContentAcrossRecomposes()
{
  ParkedFactRecord contentRecord;
  loka::core::MutableState<int> revision(0);
  g_policyRevision = &revision;
  g_policyDefaultFact = &contentRecord;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<PolicyMisplacedRecomposeBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(contentRecord.constructionCount == 1 && contentRecord.node);
  loka::app::scene::Node *const mountedNode = contentRecord.node;

  {
    loka::core::StateTrackerGuard guard(revision.trackerOwner());
    revision.set(1);
  }
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  const int constructionsAfterFirstRecompose = contentRecord.constructionCount;
  loka::app::scene::Node *const nodeAfterFirstRecompose = contentRecord.node;

  {
    loka::core::StateTrackerGuard guard(revision.trackerOwner());
    revision.set(2);
  }
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  const int constructionsAfterSecondRecompose = contentRecord.constructionCount;
  (void)mountedNode;
  (void)constructionsAfterFirstRecompose;
  (void)nodeAfterFirstRecompose;
  (void)constructionsAfterSecondRecompose;
  assert(constructionsAfterFirstRecompose == 1 &&
         constructionsAfterSecondRecompose == 1 &&
         nodeAfterFirstRecompose == mountedNode &&
         contentRecord.node == mountedNode &&
         "misplaced PolicyScope content is retained across local recomposes");
  scene.unmount();
  clearPolicyGlobals();
}

void testMisplacedPolicyScopeReconcilesReplacedInnerContent()
{
  loka::core::MutableState<int> revision(0);
  g_policyRevision = &revision;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<PolicyMisplacedReplacementBoundaryNode>()));
  mountAndAttach(scene, platform);
  LOKA_VERIFY(platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON));
  LOKA_VERIFY(!platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT));

  {
    loka::core::StateTrackerGuard guard(revision.trackerOwner());
    revision.set(1);
  }
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  platform.drainNativeRetirements();
  LOKA_VERIFY(platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT) &&
         !platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON) &&
         "a misplaced PolicyScope reconciles changed content inside a retained Fragment");
  // The request only carried PROPS; the structure-bearing apply must reach
  // the platform as a child-grade change, because real platforms gate their
  // layout/ensure pass on the flags (#277).
  LOKA_VERIFY((platform.lastOnChangeFlags() & loka::app::scene::NODE_DIRTY_CHILD) != 0 &&
              "a structure-bearing apply is a child-grade change even from a PROPS request");

  {
    loka::core::StateTrackerGuard guard(revision.trackerOwner());
    revision.set(2);
  }
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  platform.drainNativeRetirements();
  LOKA_VERIFY(platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON) &&
         !platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT));

  {
    loka::core::StateTrackerGuard guard(revision.trackerOwner());
    revision.set(3);
  }
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  platform.drainNativeRetirements();
  LOKA_VERIFY(platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT) &&
         !platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_BUTTON) &&
         "an incompatible node replaced by PolicyScope content retires exactly once");

  scene.unmount();
  clearPolicyGlobals();
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
  const unsigned long rowsBefore = platform.teardownCounters().hitRouteRemoved;

  defaultCondition.set(false);
  scopedCondition.set(false);
  LOKA_VERIFY(scene.flushInvalidation());
  platform.drainNativeRetirements();
  assertParkedTransitionTable(defaultRecord);
  assert(scopedRecord.constructionCount == 1 &&
         recordedTransitionTo(scopedRecord, loka::app::scene::NODE_FACT_RETIRED, 0));
  (void)rowsBefore;
  assert(platform.ledger().size() == 1 && platform.teardownCounters().hitRouteRemoved == rowsBefore + 1
         && "destroyOnDetach retires native ownership while default parks it");

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  scene.flushInvalidation();
  scopedCondition.set(true);
  LOKA_VERIFY(scene.flushInvalidation());
  const NullScenePlatformController::LedgerRow *rebuilt =
      platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT);
  assert(scopedRecord.constructionCount == 1 &&
         "destroyOnDetach does not reconstruct the expired branch definition");
  assert(scopedCurrentRecord.constructionCount == 1 &&
         "destroyOnDetach reshow constructs the current branch definition");
  (void)rebuilt;
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
  LOKA_VERIFY(scene.flushInvalidation());
  const int defaultAppliesBefore = defaultRecord.applies;
  const int scopedAppliesBefore = scopedRecord.applies;

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  scene.flushInvalidation();
  (void)defaultAppliesBefore;
  assert(defaultRecord.value == 0 &&
         defaultRecord.applies == defaultAppliesBefore &&
         "default policy defers parked-child reconciliation");
  (void)scopedAppliesBefore;
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
  const unsigned long rowsBefore = platform.teardownCounters().hitRouteRemoved;

  g_policyDestroyStdNode->hideBoth();
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
    platform.drainNativeRetirements();
  }
  assertParkedTransitionTable(defaultRecord);
  assert(recordedTransitionTo(scopedRecord, loka::app::scene::NODE_FACT_RETIRED, 0));
  (void)rowsBefore;
  assert(platform.ledger().size() == 1 && platform.teardownCounters().hitRouteRemoved == rowsBefore + 1);

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
  (void)defaultAppliesBefore;
  assert(defaultRecord.value == 0 &&
         defaultRecord.applies == defaultAppliesBefore);
  (void)scopedAppliesBefore;
  assert(scopedRecord.value == 1 &&
         scopedRecord.applies > scopedAppliesBefore &&
         "compose-once delivery reconciles the parked child at the door");
  scene.unmount();
  clearPolicyGlobals();
}

void testOpenFileDialogPresentationPoliciesInRecomposingBoundary()
{
  DialogPresentationRecord defaultRecord;
  DialogPresentationRecord destroyRecord;
  loka::core::MutableState<bool> defaultShown(false);
  loka::core::MutableState<bool> destroyShown(false);
  g_dialogDefaultShown = &defaultShown;
  g_dialogDestroyShown = &destroyShown;
  g_dialogDefaultRecord = &defaultRecord;
  g_dialogDestroyRecord = &destroyRecord;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<DialogPresentationRecomposeHarnessNode>()));
  mountAndAttach(scene, platform);

  defaultShown.set(true);
  destroyShown.set(true);
  LOKA_VERIFY(scene.flushInvalidation());
  assertDialogPresentationPolicyInitialShow(defaultRecord, destroyRecord);
  DialogPresentationProbeNode *defaultInstance = defaultRecord.node;
  const int defaultInstanceId = defaultRecord.node->instanceId();
  const int destroyInstanceId = destroyRecord.node->instanceId();
  defaultRecord.node->setBranchValue(73);
  destroyRecord.node->setBranchValue(73);

  defaultShown.set(false);
  destroyShown.set(false);
  LOKA_VERIFY(scene.flushInvalidation());
  assertDialogPresentationPolicyDetached(defaultRecord,
                                         destroyRecord,
                                         defaultInstance);
  drainDialogRetirement(scene);
  assert(destroyRecord.destructionCount == 1 &&
         destroyRecord.node == 0 &&
         "destroyOnDetach reclaims the retired dialog instance at the next clock boundary");

  defaultShown.set(true);
  destroyShown.set(true);
  LOKA_VERIFY(scene.flushInvalidation());
  assertDialogPresentationPolicyReentered(defaultRecord,
                                          destroyRecord,
                                          defaultInstance,
                                          defaultInstanceId,
                                          destroyInstanceId);

  scene.unmount();
  clearDialogPresentationGlobals();
}

void testOpenFileDialogPresentationPoliciesInComposeOnceBoundary()
{
  DialogPresentationRecord defaultRecord;
  DialogPresentationRecord destroyRecord;
  g_dialogDefaultRecord = &defaultRecord;
  g_dialogDestroyRecord = &destroyRecord;

  NullScenePlatformController platform;
  loka::app::scene::NodeDefinition<DialogPresentationComposeOnceBoundaryProps,
                                   DialogPresentationComposeOnceBoundaryNode> *root =
      new loka::app::scene::NodeDefinition<DialogPresentationComposeOnceBoundaryProps,
                                           DialogPresentationComposeOnceBoundaryNode>(
          DialogPresentationComposeOnceBoundaryProps());
  loka::app::scene::Scene scene(root);
  mountAndAttach(scene, platform);
  assert(g_dialogComposeOnceNode);

  g_dialogComposeOnceNode->showBoth();
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  assertDialogPresentationPolicyInitialShow(defaultRecord, destroyRecord);
  DialogPresentationProbeNode *defaultInstance = defaultRecord.node;
  const int defaultInstanceId = defaultRecord.node->instanceId();
  const int destroyInstanceId = destroyRecord.node->instanceId();
  defaultRecord.node->setBranchValue(73);
  destroyRecord.node->setBranchValue(73);

  g_dialogComposeOnceNode->hideBoth();
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  assertDialogPresentationPolicyDetached(defaultRecord,
                                         destroyRecord,
                                         defaultInstance);
  drainDialogRetirement(scene);
  assert(destroyRecord.destructionCount == 1 &&
         destroyRecord.node == 0 &&
         "compose-once destroyOnDetach reclaims the retired dialog instance");

  g_dialogComposeOnceNode->showBoth();
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  assertDialogPresentationPolicyReentered(defaultRecord,
                                          destroyRecord,
                                          defaultInstance,
                                          defaultInstanceId,
                                          destroyInstanceId);

  scene.unmount();
  clearDialogPresentationGlobals();
}

void testOpenFileDialogRequiresCompletionBinding()
{
  loka::core::MutableState<loka::app::FileChooserResult> result;
  loka::app::OpenFileDialogNode resultNode((loka::app::OpenFileDialogProps().result(&result)));
  loka::core::EmitterState onResult;
  loka::app::OpenFileDialogNode eventNode((loka::app::OpenFileDialogProps().onResult(&onResult)));
  (void)resultNode;
  (void)eventNode;

#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0)
  {
    loka::app::OpenFileDialogNode dialog((loka::app::OpenFileDialogProps()));
    (void)dialog;
    _exit(0);
  }
  int status = 0;
  LOKA_VERIFY(waitpid(child, &status, 0) == child);
  LOKA_VERIFY(WIFSIGNALED(status));
  LOKA_VERIFY(WTERMSIG(status) == SIGABRT && "OpenFileDialog requires result or onResult completion delivery");
#endif
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
    LOKA_VERIFY(scene.flushInvalidation());
  }
  assert(!scene.hasPendingInvalidation() &&
         "a single Show flip settles in one scheduled apply");

  NativeContextCallCounts afterShow(platform);
  for (int i = 0; i < 4; ++i)
  {
    requestChildPump(scene, platform);
    assert(!scene.hasPendingInvalidation() &&
           "a re-pump with no state change must not re-arm invalidation");
    NativeContextCallCounts rePumped(platform);
    assert(rePumped == afterShow &&
           "a re-pump with no state change must not churn native structure");
  }

  g_step4ShapeNode->toggleSummary(false);
  if (scene.hasPendingInvalidation())
  {
    LOKA_VERIFY(scene.flushInvalidation());
  }
  NativeContextCallCounts afterHide(platform);
  for (int i = 0; i < 4; ++i)
  {
    requestChildPump(scene, platform);
    NativeContextCallCounts rePumped(platform);
    assert(rePumped == afterHide &&
           "re-pumps while hidden must not churn native structure");
  }

  g_step4ShapeNode->toggleSummary(true);
  if (scene.hasPendingInvalidation())
  {
    LOKA_VERIFY(scene.flushInvalidation());
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
  LOKA_VERIFY(scene.flushInvalidation());
  condition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  assert(whenTrue.constructionCount == 1 &&
         whenFalse.constructionCount == 1 &&
         "both pre-removal branches materialize exactly once");

  revision.set(1);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  if (scene.hasPendingInvalidation())
  {
    scene.flushInvalidation();
  }
  assert(!scene.hasPendingInvalidation() &&
         "seat retirement drains before the same value key is re-added");

  revision.set(2);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  assert(whenTrue.constructionCount == 2 && whenTrue.node &&
         whenTrue.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "re-adding a dead seat must materialize a fresh active branch");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
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
  LOKA_VERIFY(scene.flushInvalidation());
  assert(shown.constructionCount == 1 && shown.node &&
         shown.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "a seat mounted on its null side must materialize when shown");

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  assert(shown.node->lifecycleFact() ==
             loka::app::scene::NODE_FACT_DETACHED_RETAINED &&
         "the materialized branch parks when returning to the null side");

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
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
  LOKA_VERIFY(scene.flushInvalidation());
  (void)original;
  assert(original->lifecycleFact() ==
             loka::app::scene::NODE_FACT_DETACHED_RETAINED &&
         "flipping to a null branch must park the shown branch");

  condition.set(true);
  assert(scene.hasPendingInvalidation());
  LOKA_VERIFY(scene.flushInvalidation());
  assert(shown.constructionCount == 1 && shown.node == original &&
         original->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "the shown branch must reenter from the null side without reconstruction");

  scene.unmount();
  clearNullConditionalBranchGlobals();
}

namespace
{
  /** A window that answers some axes and not others, which is the ordinary
      case rather than a contrived one: Classic has no appearance to report,
      and a pre-Windows-10 system has no light/dark setting. Used to pin that
      the availability view reports exactly what the queries answer. */
  class PartialDisplayFactsWindow : public NullWindow
  {
  public:
    PartialDisplayFactsWindow(PlatformContext *context, const WindowProps &props)
        : NullWindow(context, props)
    {
    }
    virtual bool queryDisplayScalePercent(int &out) const
    {
      out = 200;
      return true;
    }
    virtual bool queryDisplayDepth(int &out) const
    {
      out = 8;
      return true;
    }
    // queryDisplayAppearance is deliberately left declining.
  };
} // namespace

void testWindowWithoutDisplayFactsDeclinesEveryAxis()
{
  NullPlatformContext platformContext;
  WindowProps props;
  NullWindow window(&platformContext, props);

  // The sentinels must survive a declined query. A caller that ignored the
  // return value would otherwise read a value the platform never supplied,
  // which is the failure this shape exists to prevent.
  int scalePercent = -1;
  LOKA_VERIFY(!window.queryDisplayScalePercent(scalePercent));
  assert(scalePercent == -1);
  int depth = -1;
  LOKA_VERIFY(!window.queryDisplayDepth(depth));
  assert(depth == -1);
  Window::DisplayAppearance appearance = Window::DISPLAY_APPEARANCE_DARK;
  (void)appearance;
  LOKA_VERIFY(!window.queryDisplayAppearance(appearance));
  assert(appearance == Window::DISPLAY_APPEARANCE_DARK && "a declined appearance query must not be answered as light");

  for (int i = 0; i < Window::DISPLAY_FEATURE_COUNT; ++i)
  {
    assert(!window.hasDisplayFeature(static_cast<Window::DisplayFeature>(i)));
  }
}

void testDisplayFeatureAvailabilityFollowsTheQueriesThatAnswer()
{
  NullPlatformContext platformContext;
  WindowProps props;
  PartialDisplayFactsWindow window(&platformContext, props);

  // Availability is derived from the queries, so a platform that answers two
  // of three axes reports exactly those two without maintaining a second list.
  assert(window.hasDisplayFeature(Window::DISPLAY_FEATURE_SCALE));
  assert(window.hasDisplayFeature(Window::DISPLAY_FEATURE_DEPTH));
  assert(!window.hasDisplayFeature(Window::DISPLAY_FEATURE_APPEARANCE));

  int scalePercent = 0;
  LOKA_VERIFY(window.queryDisplayScalePercent(scalePercent));
  assert(scalePercent == 200);
  int depth = 0;
  LOKA_VERIFY(window.queryDisplayDepth(depth));
  assert(depth == 8);

  // The sentinel is not an axis and must not report as one.
  assert(!window.hasDisplayFeature(Window::DISPLAY_FEATURE_COUNT));
}

namespace
{
  // ScrollBar contract fixtures. The declaration is rebuilt from these
  // globals on every recompose so a test can change the *static* props
  // (range, orientation, steps) and observe the projection follow.
  loka::core::MutableState<int> *g_scrollBarValue = 0;
  loka::core::MutableState<bool> *g_scrollBarEnabled = 0;
  loka::core::EmitterState *g_scrollBarOnChange = 0;
  loka::core::MutableState<int> *g_scrollBarRevision = 0;
  bool g_scrollBarPresent = true;
  int g_scrollBarMin = 0;
  int g_scrollBarMax = 0;
  loka::app::ScrollBarOrientation g_scrollBarOrientation = loka::app::SCROLL_BAR_VERTICAL;
  int g_scrollBarLineStep = 1;
  int g_scrollBarPageStep = 1;
  /** When false the declaration calls no step setter at all, so a test can
      pin the constructor defaults rather than the fixture's re-statement of
      them. */
  bool g_scrollBarDeclareSteps = true;

  class ScrollBarBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ScrollBarBoundaryNode> ScrollBarBoundaryProps;

  class ScrollBarBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<ScrollBarBoundaryNode, ScrollBarBoundaryProps>
  {
  public:
    explicit ScrollBarBoundaryNode(const ScrollBarBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<ScrollBarBoundaryNode, ScrollBarBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_scrollBarRevision)
      {
        registrar.markDirtyOnChange(g_scrollBarRevision, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_scrollBarPresent)
      {
        loka::app::ScrollBarDefinition bar(g_scrollBarValue);
        bar.range(g_scrollBarMin, g_scrollBarMax)
            .orientation(g_scrollBarOrientation)
            .enabled(g_scrollBarEnabled)
            .onChange(g_scrollBarOnChange);
        if (g_scrollBarDeclareSteps)
        {
          bar.lineStep(g_scrollBarLineStep).pageStep(g_scrollBarPageStep);
        }
        root << bar;
      }
      composition.declare(root);
    }
  };

  void resetScrollBarFixture()
  {
    g_scrollBarValue = 0;
    g_scrollBarEnabled = 0;
    g_scrollBarOnChange = 0;
    g_scrollBarRevision = 0;
    g_scrollBarPresent = true;
    g_scrollBarMin = 0;
    g_scrollBarMax = 0;
    g_scrollBarOrientation = loka::app::SCROLL_BAR_VERTICAL;
    g_scrollBarLineStep = 1;
    g_scrollBarPageStep = 1;
    g_scrollBarDeclareSteps = true;
  }

  NullScrollBarContext *findScrollBarContext(const NullScenePlatformController &platform)
  {
    const NullScenePlatformController::LedgerRow *row =
        platform.findLedgerRow(NullScenePlatformController::CONTROL_RECIPE_SCROLL_BAR);
    if (!row || !row->handle)
    {
      return 0;
    }
    return static_cast<NullScrollBarContext *>(row->handle->owner);
  }

  // Witness for the settle order: reads the bound value at the instant
  // onChange fires. If the emit ran before the write, this records the old
  // value and the assertion fails.
  struct ScrollBarChangeWitness
  {
    ScrollBarChangeWitness()
        : emitCount(0),
          valueSeenAtEmit(-1)
    {
    }

    unsigned long emitCount;
    int valueSeenAtEmit;

    static void Thunk(void *userData)
    {
      ScrollBarChangeWitness *self = static_cast<ScrollBarChangeWitness *>(userData);
      if (!self)
      {
        return;
      }
      ++self->emitCount;
      self->valueSeenAtEmit = g_scrollBarValue ? g_scrollBarValue->get() : -1;
    }
  };
} // namespace

void testNullPlatformContract_S1_scrollBarProjectsAndRetiresIntoItsOwnBucket()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(0);
  loka::core::MutableState<int> revision(0);
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarMax = 4;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);

  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].recipe == NullScenePlatformController::CONTROL_RECIPE_SCROLL_BAR);
  assert(platform.ledger()[0].visible);
  assert(platform.eventCount(NullScenePlatformController::EVENT_CONTROL_SHOWN) == 1);

  g_scrollBarPresent = false;
  revision.set(1);
  requestChildPump(scene, platform);

  // A retired scroll bar must not be paid back out as a button or an edit
  // field: the recipe is the exact-match key.
  assert(platform.ledger().empty());
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_SCROLL_BAR).depth == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT).depth == 0);
  resetScrollBarFixture();
}

void testNullPlatformContract_S1_safePointDrainsScrollBarIntoOwnBucket()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(0);
  loka::core::MutableState<int> revision(0);
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarMax = 4;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);

  retireProjectedContextsWithoutApply(scene, platform);
  assert(platform.hasPendingSync() && "synchronize must receive pending retired-handle intake");
  synchronizeThenDrainAtSafePoint(platform);

  assert(platform.ledger().empty());
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_SCROLL_BAR).depth == 1);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_BUTTON).depth == 0);
  assert(platform.bucketStats(NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT).depth == 0);
  resetScrollBarFixture();
}

void testNullPlatformContract_S2_heldArrowSettlesExactlyOnceAfterTheStateWrite()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(0);
  loka::core::MutableState<int> revision(0);
  loka::core::EmitterState onChange;
  ScrollBarChangeWitness witness;
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarOnChange = &onChange;
  g_scrollBarMax = 4;
  onChange.bind(&ScrollBarChangeWitness::Thunk, &witness, false);

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  assert(context);

  // App -> control: a write to the binding moves what the control shows.
  value.set(3);
  requestChildPump(scene, platform);
  assert(context->displayedValue() == 3);
  assert(witness.emitCount == 0 && "an app-side write is not a user change");

  // Control -> app: three action-proc ticks of one held arrow settle once.
  context->simulatePress(NullScrollBarContext::PART_LINE_DOWN, 3);
  assert(context->displayedValue() == 4 && "the hold clamps at max, it does not wrap");
  assert(value.get() == 4);
  assert(context->stateWriteCount() == 1);
  assert(witness.emitCount == 1);
  assert(witness.valueSeenAtEmit == 4 && "onChange must observe the value already written");

  onChange.unbind(&ScrollBarChangeWitness::Thunk, &witness);
  resetScrollBarFixture();
}

void testNullPlatformContract_S2_directProjectionRefreshesBoundDisplayValue()
{
  loka::core::MutableState<int> value(0);
  loka::app::ScrollBarProps props(&value);
  props.range(0, 4);
  NullScenePlatformController platform;
  loka::app::ScrollBarNode scrollBar(props);
  loka::app::scene::LayoutState state;
  state.width = 100;
  state.height = 20;

  platform.projectLayoutForTesting(&scrollBar, state);
  NullScrollBarContext *context =
      static_cast<NullScrollBarContext *>(scrollBar.getContext());
  (void)context;
  assert(context);
  assert(context->displayedValue() == 0);

  value.set(3);
  platform.projectLayoutForTesting(&scrollBar, state);
  assert(context->displayedValue() == 3);
  assert(context->stateWriteCount() == 0);
}

void testNullPlatformContract_S3_declaredOrientationAndStepsDriveTheControl()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(6);
  loka::core::MutableState<int> revision(0);
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarMax = 20;
  g_scrollBarOrientation = loka::app::SCROLL_BAR_HORIZONTAL;
  g_scrollBarLineStep = 2;
  g_scrollBarPageStep = 5;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  assert(context);

  // Orientation is carried, never inferred from the projected geometry.
  assert(context->orientation() == loka::app::SCROLL_BAR_HORIZONTAL);
  assert(loka::app::ScrollBarDefinition(&value).props.orientation_ == loka::app::SCROLL_BAR_VERTICAL &&
         "vertical is the default; horizontal must be asked for");

  context->simulatePress(NullScrollBarContext::PART_LINE_UP, 1);
  assert(value.get() == 4);
  requestChildPump(scene, platform);

  context->simulatePress(NullScrollBarContext::PART_PAGE_DOWN, 1);
  assert(value.get() == 9);
  requestChildPump(scene, platform);

  context->simulateThumbDragTo(17);
  assert(value.get() == 17);
  assert(context->stateWriteCount() == 3 && "one settle per release, whichever part was pressed");
  resetScrollBarFixture();
}

void testNullPlatformContract_S4_unscrollableAndDisabledBarsAreInactive()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(0);
  loka::core::MutableState<int> revision(0);
  loka::core::MutableState<bool> enabled(true);
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarEnabled = &enabled;
  // min == max: a scroll bar over an empty document. Legal scene, inactive
  // presentation -- not a refusal.
  g_scrollBarMin = 0;
  g_scrollBarMax = 0;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  assert(context);
  assert(platform.ledger().size() == 1 && "an unscrollable bar still exists");
  assert(!context->active());

  context->simulatePress(NullScrollBarContext::PART_LINE_DOWN, 1);
  assert(value.get() == 0);
  assert(context->stateWriteCount() == 0);

  // Widening the range revives it through the same recompose path.
  g_scrollBarMax = 4;
  revision.set(1);
  requestChildPump(scene, platform);
  context = findScrollBarContext(platform);
  assert(context && context->active());

  // enabled is the other reason for the same inactive presentation.
  enabled.set(false);
  requestChildPump(scene, platform);
  assert(!context->active());
  context->simulatePress(NullScrollBarContext::PART_LINE_DOWN, 1);
  assert(value.get() == 0);
  assert(context->stateWriteCount() == 0);
  resetScrollBarFixture();
}

void testNullPlatformContract_S5_recomposedRangeClampsTheDisplayWithoutWritingBack()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(9);
  loka::core::MutableState<int> revision(0);
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarMax = 20;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  (void)context;
  assert(context);
  assert(context->maximum() == 20);
  assert(context->displayedValue() == 9);

  // range() is static prop data, so a narrower range arrives by recompose.
  g_scrollBarMax = 4;
  revision.set(1);
  requestChildPump(scene, platform);
  context = findScrollBarContext(platform);
  assert(context);
  assert(context->maximum() == 4);

  // Out-of-range bound value: the view clamps, the binding is left alone --
  // the same shape PopupMenu uses for an out-of-range selectedIndex.
  // Composing a scene must not mutate application state.
  assert(context->displayedValue() == 4);
  assert(value.get() == 9);
  assert(context->stateWriteCount() == 0);
  resetScrollBarFixture();
}

void testNullPlatformContract_S5_directProjectionClampsNarrowedRangeWithoutWriteBack()
{
  loka::core::MutableState<int> value(9);
  loka::app::ScrollBarProps props(&value);
  props.range(0, 20);
  NullScenePlatformController platform;
  loka::app::ScrollBarNode scrollBar(props);
  loka::app::scene::LayoutState state;
  state.width = 100;
  state.height = 20;

  platform.projectLayoutForTesting(&scrollBar, state);
  NullScrollBarContext *context =
      static_cast<NullScrollBarContext *>(scrollBar.getContext());
  (void)context;
  assert(context);
  assert(context->displayedValue() == 9);

  scrollBar.props.max_ = 4;
  platform.projectLayoutForTesting(&scrollBar, state);
  assert(context->maximum() == 4);
  assert(context->displayedValue() == 4);
  assert(value.get() == 9);
  assert(context->stateWriteCount() == 0);
}

void testNullPlatformContract_S6_gestureSettlingWhereItStartedPublishesNothing()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(2);
  loka::core::MutableState<int> revision(0);
  loka::core::EmitterState onChange;
  ScrollBarChangeWitness witness;
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarOnChange = &onChange;
  g_scrollBarMax = 4;
  onChange.bind(&ScrollBarChangeWitness::Thunk, &witness, false);

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  assert(context);

  // A drag that lands back on its origin is Classic's cancelled outline
  // drag: a gesture the user abandoned, not a decision. Publishing it would
  // fire a page re-read for nothing (both arms carry the appliedValue gate).
  context->simulateThumbDragTo(2);
  assert(context->stateWriteCount() == 0);
  assert(witness.emitCount == 0);
  assert(value.get() == 2);

  // A real move still publishes -- the gate is about "unchanged", not about
  // suppressing commits generally.
  context->simulateThumbDragTo(4);
  assert(context->stateWriteCount() == 1);
  assert(witness.emitCount == 1);

  // An arrow held against the end of the range settles where it started.
  context->simulatePress(NullScrollBarContext::PART_LINE_DOWN, 5);
  assert(context->displayedValue() == 4);
  assert(context->stateWriteCount() == 1 &&
         "a hold clamped at the end publishes nothing");
  assert(witness.emitCount == 1);

  onChange.unbind(&ScrollBarChangeWitness::Thunk, &witness);
  resetScrollBarFixture();
}

void testNullPlatformContract_S7_nothingCrossesIntoLokaBeforeTheRelease()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(0);
  loka::core::MutableState<int> revision(0);
  loka::core::EmitterState onChange;
  ScrollBarChangeWitness witness;
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarOnChange = &onChange;
  g_scrollBarMax = 40;
  g_scrollBarLineStep = 2;
  g_scrollBarPageStep = 10;
  onChange.bind(&ScrollBarChangeWitness::Thunk, &witness, false);

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  assert(context);

  // The ruling, observed at the joint the completed-gesture seams hide:
  // ticks move only what the user sees, and the binding does not hear a
  // word of it until the release. Every part, same sentence.

  // Line arrows.
  context->pressTick(NullScrollBarContext::PART_LINE_DOWN);
  context->pressTick(NullScrollBarContext::PART_LINE_DOWN);
  assert(context->displayedValue() == 4);
  assert(value.get() == 0 && context->stateWriteCount() == 0 &&
         "tracking is visual only");
  assert(witness.emitCount == 0);
  context->release();
  assert(value.get() == 4 && context->stateWriteCount() == 1);
  assert(witness.emitCount == 1 && witness.valueSeenAtEmit == 4 &&
         "the write precedes the emit on the arrow path");

  // Page areas.
  context->pressTick(NullScrollBarContext::PART_PAGE_DOWN);
  assert(context->displayedValue() == 14);
  assert(value.get() == 4 && context->stateWriteCount() == 1);
  assert(witness.emitCount == 1);
  context->release();
  assert(value.get() == 14 && context->stateWriteCount() == 2);
  assert(witness.emitCount == 2 && witness.valueSeenAtEmit == 14 &&
         "the write precedes the emit on the page path");

  // The thumb.
  context->dragThumbTo(33);
  assert(context->displayedValue() == 33);
  assert(value.get() == 14 && context->stateWriteCount() == 2 &&
         "the outline drag publishes nothing");
  assert(witness.emitCount == 2);
  context->release();
  assert(value.get() == 33 && context->stateWriteCount() == 3);
  assert(witness.emitCount == 3 && witness.valueSeenAtEmit == 33 &&
         "the write precedes the emit on the thumb path");

  onChange.unbind(&ScrollBarChangeWitness::Thunk, &witness);
  resetScrollBarFixture();
}

void testNullPlatformContract_S8_rangeEdgesClampTheDisplayAndOnlyTheDisplay()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(2);
  loka::core::MutableState<int> revision(0);
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarMin = 5;
  g_scrollBarMax = 20;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  assert(context);

  // Below the floor: the S5 rule from the other side. The view shows the
  // floor, the binding keeps what the application put there.
  assert(context->displayedValue() == 5);
  assert(value.get() == 2);
  assert(context->stateWriteCount() == 0);

  // An arrow pressed against the floor settles where it started -- the S6
  // gate at the lower edge.
  context->simulatePress(NullScrollBarContext::PART_LINE_UP, 3);
  assert(context->displayedValue() == 5);
  assert(context->stateWriteCount() == 0);

  // A negative range is a range like any other; stepping is arithmetic on
  // values, not on distances from zero.
  g_scrollBarMin = -10;
  g_scrollBarMax = -2;
  revision.set(1);
  requestChildPump(scene, platform);
  context = findScrollBarContext(platform);
  assert(context);
  assert(context->displayedValue() == -2 && "2 clamps to the negative ceiling");
  context->simulatePress(NullScrollBarContext::PART_LINE_UP, 1);
  assert(context->displayedValue() == -3);
  assert(value.get() == -3 && context->stateWriteCount() == 1);

  // A reversed range has nowhere to go: the inactive presentation, the same
  // one min == max earns, and a gesture publishes nothing.
  g_scrollBarMin = 5;
  g_scrollBarMax = 2;
  revision.set(2);
  requestChildPump(scene, platform);
  context = findScrollBarContext(platform);
  assert(context);
  assert(!context->active());
  assert(context->displayedValue() == 5 && "a reversed range presents its min");
  context->simulatePress(NullScrollBarContext::PART_LINE_DOWN, 1);
  assert(context->stateWriteCount() == 1 && "no publish through a reversed range");
  assert(value.get() == -3);

  resetScrollBarFixture();
}

void testNullPlatformContract_S8_directProjectionRefreshesRangeEdges()
{
  loka::core::MutableState<int> value(2);
  loka::app::ScrollBarProps props(&value);
  props.range(5, 20);
  NullScenePlatformController platform;
  loka::app::ScrollBarNode scrollBar(props);
  loka::app::scene::LayoutState state;
  state.width = 100;
  state.height = 20;

  platform.projectLayoutForTesting(&scrollBar, state);
  NullScrollBarContext *context =
      static_cast<NullScrollBarContext *>(scrollBar.getContext());
  (void)context;
  assert(context);
  assert(context->displayedValue() == 5);

  scrollBar.props.min_ = -10;
  scrollBar.props.max_ = -2;
  platform.projectLayoutForTesting(&scrollBar, state);
  assert(context->displayedValue() == -2);

  scrollBar.props.min_ = 5;
  scrollBar.props.max_ = 2;
  platform.projectLayoutForTesting(&scrollBar, state);
  assert(!context->active());
  assert(context->displayedValue() == 5);
  assert(value.get() == 2);
  assert(context->stateWriteCount() == 0);
}

void testNullPlatformContract_S9_absentBindingsKeepGesturesLocal()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> revision(0);
  loka::core::EmitterState onChange;
  ScrollBarChangeWitness witness;
  g_scrollBarValue = 0;
  g_scrollBarRevision = &revision;
  g_scrollBarOnChange = &onChange;
  g_scrollBarMax = 4;
  onChange.bind(&ScrollBarChangeWitness::Thunk, &witness, false);

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  assert(context);

  // No value binding: the bar still tracks visually -- it is a control, not
  // a refusal -- but there is no settled place for a value to land, so
  // nothing is published and onChange stays silent with it. An emitter
  // without a binding has no settled value for its handler to read, which
  // is the order the whole contract is built on.
  assert(context->displayedValue() == 0 && "an unbound bar rests on its min");
  context->simulatePress(NullScrollBarContext::PART_LINE_DOWN, 2);
  assert(context->displayedValue() == 2);
  assert(context->stateWriteCount() == 0);
  assert(witness.emitCount == 0 && "onChange without a value binding never fires");

  onChange.unbind(&ScrollBarChangeWitness::Thunk, &witness);
  resetScrollBarFixture();
}

void testNullPlatformContract_S10_stepDefaultsAreOneWithoutBeingRestated()
{
  resetScrollBarFixture();
  loka::core::MutableState<int> value(0);
  loka::core::MutableState<int> revision(0);
  g_scrollBarValue = &value;
  g_scrollBarRevision = &revision;
  g_scrollBarMax = 10;
  // The declaration names no step at all, so what moves is the constructor
  // default -- the fixture restating 1 would make this test a tautology.
  g_scrollBarDeclareSteps = false;

  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ScrollBarBoundaryNode>()));
  mountAndAttach(scene, platform);
  NullScrollBarContext *context = findScrollBarContext(platform);
  assert(context);

  context->simulatePress(NullScrollBarContext::PART_LINE_DOWN, 1);
  assert(value.get() == 1 && "the default line step is one");
  context->simulatePress(NullScrollBarContext::PART_PAGE_DOWN, 1);
  assert(value.get() == 2 && "the default page step is one");

  resetScrollBarFixture();
}
