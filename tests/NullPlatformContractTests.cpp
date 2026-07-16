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

  loka::core::MutableState<int> *g_hintRevision = 0;
  loka::app::scene::NativeLifetimeHint g_runtimeHint = loka::app::scene::NATIVE_HINT_DEFAULT;

  class HintBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HintBoundaryNode> HintBoundaryProps;

  class HintBoundaryNode
      : public RecomposingContractBoundaryNode<HintBoundaryNode, HintBoundaryProps>
  {
  public:
    explicit HintBoundaryNode(const HintBoundaryProps &props)
        : RecomposingContractBoundaryNode<HintBoundaryNode, HintBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_hintRevision)
      {
        registrar.markDirtyOnChange(g_hintRevision, loka::app::scene::NODE_DIRTY_PROPS);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      loka::app::ButtonDefinition button("hint");
      button.lifetimeHint(g_runtimeHint);
      root << button;
      composition.declare(root);
    }
  };

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
  loka::core::MutableState<int> revision(0);
  g_hintRevision = &revision;
  g_runtimeHint = loka::app::scene::NATIVE_HINT_EAGER_RELEASE;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<HintBoundaryNode>()));
  mountAndAttach(scene, platform);
  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].hint == loka::app::scene::NATIVE_HINT_EAGER_RELEASE);

  g_runtimeHint = loka::app::scene::NATIVE_HINT_DESIRE_STAY;
  revision.set(1);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
  assert(scene.flushInvalidation());

  assert(platform.ledger().size() == 1);
  assert(platform.ledger()[0].hint == loka::app::scene::NATIVE_HINT_DESIRE_STAY);
  g_hintRevision = 0;
  g_runtimeHint = loka::app::scene::NATIVE_HINT_DEFAULT;
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
