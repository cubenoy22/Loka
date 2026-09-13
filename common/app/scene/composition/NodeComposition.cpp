#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/boundary/detail/BoundaryArena.hpp"
#include "app/scene/detail/ArenaMath.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/boundary/detail/NodeBuildTicket.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/core/Window.hpp"
#include <cstdio>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      NodeComposition *NodeComposition::current_ = 0;

      NodeComposition::CompositionScope::CompositionScope(NodeComposition &composition)
          : prev_(current_)
      {
        current_ = &composition;
      }

      NodeComposition::CompositionScope::~CompositionScope()
      {
        current_ = prev_;
      }

      NodeComposition *NodeComposition::current()
      {
        return current_;
      }

      struct INestableDefinition;
      struct INestable;

      // Pass 1: Calculate total size needed for all nodes
      static size_t
      calculateTotalNodeSize(NodeDefinitionBase *def, BoundaryNode *boundary, BoundaryBranchSeatState *seatScope = 0)
      {
        if (!def)
        {
          return 0;
        }
        IBranchPolicyScopeDefinition *scope = def->asBranchPolicyScopeDefinition();
        if (scope)
        {
          return calculateTotalNodeSize(scope->scopedBranchDefinition(), boundary, seatScope);
        }
        IBranchSeatDefinition *seat = def->asBranchSeatDefinition();
        if (seat)
        {
          const BoundaryBranchSeatPlanEntry *plan = boundary ? boundary->branchSeatPlan(def, seatScope) : 0;
          assert(plan && plan->dirtySource &&
                 "branch seat requires a captured Boundary plan");
          if (!plan || !plan->dirtySource)
          {
            return 0;
          }
          return calculateTotalNodeSize(plan->hasSelectedArm ? plan->branch(plan->selectedArm).definition : 0,
                                        boundary,
                                        seat->declaredBranchSeats() ? seat->declaredBranchSeats() : seatScope);
        }
        INestableDefinition *nestableDef = def->asNestableDefinition();
        // Add size with alignment padding (worst case)
        size_t align = detail::NormalizeArenaAlign(def->nodeAlign());
        size_t total = def->nodeSize() + align;

        if (nestableDef)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            total += calculateTotalNodeSize(child, boundary, seatScope);
            child = child->nextInComposition;
          }
        }
        return total;
      }

      // Pass 2: Create nodes using arena
      static std::string makeAutoTestId(long index)
      {
        char buf[64];
        snprintf(buf, sizeof(buf), "auto-%ld", index);
        return std::string(buf);
      }

      static void assignNodeTestId(Node *node, const NodeDefinitionBase *def, long &autoIdCounter)
      {
        if (!node || !def)
        {
          return;
        }
        if (def->hasTestId())
        {
          node->setTestId(def->testIdValue());
          return;
        }
        if (def->wantsAutoTestId())
        {
          node->setTestId(makeAutoTestId(autoIdCounter++));
        }
      }

      static NodeMaterializationResult createNodeWithArena(NodeDefinitionBase *def,
                                                           NodeArena *arena,
                                                           long &autoIdCounter,
                                                           BoundaryNode *boundary,
                                                           Node *runtimeParent,
                                                           BoundaryBranchSeatRuntimeRegistrationPlan *registrations,
                                                           ComponentContext &context,
                                                           BoundaryBranchSeatState *seatScope)
      {
        if (!def)
        {
          NodeMaterializationResult empty = {0, false, false};
          return empty;
        }

        IBranchPolicyScopeDefinition *scope = def->asBranchPolicyScopeDefinition();
        if (scope)
        {
          return createNodeWithArena(scope->scopedBranchDefinition(),
                                     arena,
                                     autoIdCounter,
                                     boundary,
                                     runtimeParent,
                                     registrations,
                                     context,
                                     seatScope);
        }
        IBranchSeatDefinition *seat = def->asBranchSeatDefinition();
        if (seat)
        {
          const BoundaryBranchSeatPlanEntry *plan = boundary ? boundary->branchSeatPlan(def, seatScope) : 0;
          if (!plan || !plan->dirtySource)
          {
            assert(boundary == 0 &&
                   "a boundary-backed compose must have captured this seat's plan");
            NodeMaterializationResult missingPlan = {0, false, true};
            return missingPlan;
          }
          loka::app::FragmentDefinition emptyBranch;
          NodeDefinitionBase *branchDefinition = plan->materializedBranchDefinition(emptyBranch);
          NodeMaterializationResult active;
          if (seat->needsBranchDeclaration())
          {
            active = boundary->materializeDeclaredSeat(context, *plan, runtimeParent, registrations);
          }
          else
          {
            active = createNodeWithArena(branchDefinition,
                                         arena,
                                         autoIdCounter,
                                         boundary,
                                         runtimeParent,
                                         registrations,
                                         context,
                                         seat->declaredBranchSeats() ? seat->declaredBranchSeats() : seatScope);
          }
          if (active.root)
          {
            if (registrations)
            {
              registrations->record(*plan, runtimeParent, active.root, context.stateOwner());
            }
            else
            {
              boundary->registerMaterializedBranchSeat(*plan, runtimeParent, active.root, context.stateOwner());
            }
          }
          return active;
        }

        // Allocate from arena
        size_t nodeSize = def->nodeSize();
        size_t nodeAlign = def->nodeAlign();
        void *mem = context.nodeStorage() ? 0 : arena->allocate(nodeSize, nodeAlign);
        Node *node;
        if (context.nodeStorage())
        {
          node = context.nodeStorage()->create(*def, runtimeParent);
        }
        else if (mem)
        {
          node = def->createInPlace(mem);
          arena->registerNode(node);
        }
        else
        {
          // Fallback to regular allocation
          node = def->create();
        }
        if (!node)
        {
          NodeMaterializationResult refused = {0, true, false};
          return refused;
        }
        assignNodeTestId(node, def, autoIdCounter);

        NodeMaterializationResult result = {node, false, false};

        ComponentContext childContext(context);
        childContext.setOwner(node);
        IStateOwner *owner = node->asStateOwner();
        if (owner)
        {
          if (!owner->attachStateOwner(boundary, context.stateOwner()))
          {
            result.allocationFailed = true;
            return result;
          }
          childContext.setStateOwner(owner);
        }
        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();

        if (nestableDef && nestableNode)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            NodeMaterializationResult childResult = createNodeWithArena(
                child, arena, autoIdCounter, boundary, node, registrations, childContext, seatScope);
            result.allocationFailed = result.allocationFailed || childResult.allocationFailed;
            result.requiresBoundaryPlan =
                result.requiresBoundaryPlan || childResult.requiresBoundaryPlan;
            if (childResult.root)
            {
              nestableNode->addChild(childResult.root);
            }
            child = child->nextInComposition;
          }
        }

        return result;
      }

      // Fallback: create without arena
      static NodeMaterializationResult
      createNodeRecursive(NodeDefinitionBase *def, long &autoIdCounter, ComponentContext &context)
      {
        if (!def)
        {
          NodeMaterializationResult empty = {0, false, false};
          return empty;
        }

        IBranchPolicyScopeDefinition *scope = def->asBranchPolicyScopeDefinition();
        if (scope)
        {
          return createNodeRecursive(scope->scopedBranchDefinition(), autoIdCounter, context);
        }
        if (def->asBranchSeatDefinition())
        {
          // Local heap materialization has no captured seat plan. The caller
          // must reject the completed result rather than publish a partial tree.
          NodeMaterializationResult missingPlan = {0, false, true};
          return missingPlan;
        }

        Node *node = context.nodeStorage()
                         ? context.nodeStorage()->create(*def, context.owner())
                         : def->create();
        if (!node)
        {
          NodeMaterializationResult refused = {0, true, false};
          return refused;
        }
        assignNodeTestId(node, def, autoIdCounter);

        NodeMaterializationResult result = {node, false, false};

        ComponentContext childContext(context);
        childContext.setOwner(node);
        IStateOwner *owner = node->asStateOwner();
        if (owner)
        {
          if (!owner->attachStateOwner(context.boundary(), context.stateOwner()))
          {
            result.allocationFailed = true;
            return result;
          }
          childContext.setStateOwner(owner);
        }
        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();

        if (nestableDef && nestableNode)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            NodeMaterializationResult childResult = createNodeRecursive(child, autoIdCounter, childContext);
            result.allocationFailed = result.allocationFailed || childResult.allocationFailed;
            result.requiresBoundaryPlan =
                result.requiresBoundaryPlan || childResult.requiresBoundaryPlan;
            if (childResult.root)
            {
              nestableNode->addChild(childResult.root);
            }
            child = child->nextInComposition;
          }
        }

        return result;
      }

      void NodeComposition::noteCaptureRefusal()
      {
        if (this->context_ && this->context_->boundary())
          this->context_->boundary()->noteComposeAllocationFailure();
      }

      void NoteDefinitionCaptureRefusal()
      {
        NodeComposition *composition = NodeComposition::current();
        if (composition)
          composition->noteCaptureRefusal();
      }

      NodeMaterializationResult NodeComposition::createNodeTreeCompleted() const
      {
        assert(context_ && context_->boundary() &&
               "NodeComposition::createNodeTreeCompleted requires BoundaryNode context");
        NodeMaterializationResult result =
            this->createNodeFromDefinitionResult(this->root());
        if (result.requiresBoundaryPlan)
        {
          context_->boundary()->noteComposeBoundaryPlanRequired();
        }
        if (result.allocationFailed)
        {
          context_->boundary()->noteComposeAllocationFailure();
        }
        return result;
      }

      static void assignDefinitionSeatSlots(NodeDefinitionBase *definition, int &nextSlot)
      {
        if (!definition)
        {
          return;
        }
        definition->setCompositionSeatSlot(nextSlot++);
        INestableDefinition *nestable = definition->asNestableDefinition();
        if (nestable)
        {
#ifndef NDEBUG
          // Misuse detection only: the fully-tagged and duplicate-key asserts
          // are this scan's sole effects, so release builds skip it entirely.
          // The release wall for a duplicate branch-seat key is where the key
          // is minted (BoundaryBranchSeatState::captureDefinition).
          bool requiresFullyTaggedSiblings = false;
          for (NodeDefinitionBase *candidate = nestable->childrenHead();
               candidate;
               candidate = candidate->nextInComposition)
          {
            if (candidate->requiresFullyTaggedSiblings())
            {
              requiresFullyTaggedSiblings = true;
              break;
            }
          }
          if (requiresFullyTaggedSiblings)
          {
            for (NodeDefinitionBase *candidate = nestable->childrenHead();
                 candidate;
                 candidate = candidate->nextInComposition)
            {
              assert(candidate->nodeTag() != NODE_TAG_NONE &&
                     "a sibling list containing a BoundarySection must be fully tagged");
            }
          }
          for (NodeDefinitionBase *candidate = nestable->childrenHead();
               candidate;
               candidate = candidate->nextInComposition)
          {
            if (candidate->nodeTag() == NODE_TAG_NONE)
            {
              continue;
            }
            for (NodeDefinitionBase *sibling = candidate->nextInComposition;
                 sibling;
                 sibling = sibling->nextInComposition)
            {
              if (candidate->nodeTag() == sibling->nodeTag() &&
                  (candidate->requiresUniqueSiblingTag() || sibling->requiresUniqueSiblingTag()))
              {
                assert(false &&
                       "sibling BoundarySections and branch seats require unique value keys");
              }
            }
          }
#endif
          for (NodeDefinitionBase *child = nestable->childrenHead(); child; child = child->nextInComposition)
          {
            assignDefinitionSeatSlots(child, nextSlot);
          }
        }
        IBranchSeatDefinition *seat = definition->asBranchSeatDefinition();
        if (seat)
        {
          // Indexed arms may be empty (null) anywhere in the list; walk every
          // index so the arms behind an empty one still get their own slots.
          for (unsigned arm = 0; arm < seat->armCount(); ++arm)
          {
            assignDefinitionSeatSlots(seat->armDefinition(arm), nextSlot);
          }
          return;
        }
        for (unsigned i = 0; NodeDefinitionBase *branch = definition->retainedDefinitionBranch(i); ++i)
        {
          assignDefinitionSeatSlots(branch, nextSlot);
        }
      }

      void NodeComposition::assignCompositionSeatSlots()
      {
        int nextSlot = 0;
        assignDefinitionSeatSlots(this->root_, nextSlot);
      }

      NodeMaterializationResult NodeComposition::createNodeFromDefinitionResult(
          NodeDefinitionBase *root, Node *runtimeParent, BoundaryBranchSeatState *seatScope) const
      {
        if (!root)
        {
          NodeMaterializationResult empty = {0, false, false};
          return empty;
        }

        // Try to use arena if boundary is available
        if (context_)
        {
          BoundaryNode *bnd = context_->boundary();
          if (bnd)
          {
            NodeArena *arena = bnd->nodeArena();
            if (!context_->nodeStorage() && !arena->hasCapacity())
            {
              // An arena reservation refusal is storage-strategy degradation,
              // not a logical materialization failure. Only a refusal to
              // materialize at BOTH doors — the arena and the final heap door —
              // becomes a compose failure (#132 ruling 3).
              size_t totalSize = calculateTotalNodeSize(root, bnd, seatScope);
              arena->reserve(totalSize);
            }
            long autoIdCounter = 1;
            return createNodeWithArena(root,
                                       arena,
                                       autoIdCounter,
                                       bnd,
                                       runtimeParent ? runtimeParent : bnd,
                                       this->branchSeatRegistrations_,
                                       *context_,
                                       seatScope);
          }
        }

        ComponentContext fallbackContext;
        return NodeComposition::createNodeWithoutArenaResult(root, context_ ? *context_ : fallbackContext);
      }

      NodeMaterializationResult NodeComposition::createNodeWithoutArenaResult(NodeDefinitionBase *definition,
                                                                              ComponentContext &context)
      {
        long autoIdCounter = 1;
        return createNodeRecursive(definition, autoIdCounter, context);
      }

      BoundaryNode *NodeComposition::boundary() const
      {
        assert(context_ && "NodeComposition::boundary requires ComponentContext");
        BoundaryNode *boundary = context_->boundary();
        assert(boundary && "NodeComposition::boundary requires BoundaryNode");
        return boundary;
      }

      Scene *NodeComposition::scene() const
      {
        assert(context_ && "NodeComposition::scene requires ComponentContext");
        Scene *scene = context_->scene();
        assert(scene && "NodeComposition::scene requires Scene");
        return scene;
      }

      ::Window *NodeComposition::window() const
      {
        assert(context_ && "NodeComposition::window requires ComponentContext");
        ::Window *window = context_->window();
        assert(window && "NodeComposition::window requires Window");
        return window;
      }

    } // namespace scene
  } // namespace app
} // namespace loka
