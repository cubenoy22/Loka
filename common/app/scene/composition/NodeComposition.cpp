#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/boundary/detail/BoundaryArena.hpp"
#include "app/scene/detail/ArenaMath.hpp"
#include "app/scene/Scene.hpp"
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
      static size_t calculateTotalNodeSize(NodeDefinitionBase *def, BoundaryNode *boundary)
      {
        if (!def)
        {
          return 0;
        }
        assert(!def->asBranchPolicyScopeDefinition() &&
               "PolicyScope is legal only as the immediate branch root of a conditional seat");
        IBranchSeatDefinition *seat = def->asBranchSeatDefinition();
        if (seat)
        {
          const BoundaryBranchSeatPlanEntry *plan = boundary ? boundary->branchSeatPlan(def) : 0;
          assert(plan && plan->condition && "conditional seat requires a captured Boundary plan");
          if (!plan || !plan->condition)
          {
            return 0;
          }
          return calculateTotalNodeSize(plan->branch(plan->condition->get()).definition, boundary);
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
            total += calculateTotalNodeSize(child, boundary);
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
                                                           Node *runtimeParent)
      {
        if (!def)
        {
          NodeMaterializationResult empty = {0, false};
          return empty;
        }

        assert(!def->asBranchPolicyScopeDefinition() &&
               "PolicyScope is legal only as the immediate branch root of a conditional seat");
        IBranchSeatDefinition *seat = def->asBranchSeatDefinition();
        if (seat)
        {
          const BoundaryBranchSeatPlanEntry *plan = boundary ? boundary->branchSeatPlan(def) : 0;
          assert(plan && plan->condition && "conditional seat requires a captured Boundary plan");
          if (!plan || !plan->condition)
          {
            NodeMaterializationResult missingPlan = {0, false};
            return missingPlan;
          }
          const bool condition = plan->condition->get();
          loka::app::FragmentDefinition emptyBranch;
          NodeDefinitionBase *branchDefinition =
              plan->materializedBranchDefinition(condition, emptyBranch);
          NodeMaterializationResult active = createNodeWithArena(branchDefinition,
                                                                 arena,
                                                                 autoIdCounter,
                                                                 boundary,
                                                                 runtimeParent);
          if (active.root)
          {
            boundary->registerMaterializedBranchSeat(*plan,
                                                     runtimeParent,
                                                     active.root,
                                                     condition);
          }
          return active;
        }

        // Allocate from arena
        size_t nodeSize = def->nodeSize();
        size_t nodeAlign = def->nodeAlign();
        void *mem = arena->allocate(nodeSize, nodeAlign);
        Node *node;
        if (mem)
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
          NodeMaterializationResult refused = {0, true};
          return refused;
        }
        assignNodeTestId(node, def, autoIdCounter);

        NodeMaterializationResult result = {node, false};

        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();

        if (nestableDef && nestableNode)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            NodeMaterializationResult childResult = createNodeWithArena(child,
                                                                        arena,
                                                                        autoIdCounter,
                                                                        boundary,
                                                                        node);
            result.allocationFailed = result.allocationFailed || childResult.allocationFailed;
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
      static NodeMaterializationResult createNodeRecursive(NodeDefinitionBase *def,
                                                           long &autoIdCounter,
                                                           BoundaryNode *boundary,
                                                           Node *runtimeParent)
      {
        if (!def)
        {
          NodeMaterializationResult empty = {0, false};
          return empty;
        }

        assert(!def->asBranchPolicyScopeDefinition() &&
               "PolicyScope is legal only as the immediate branch root of a conditional seat");
        IBranchSeatDefinition *seat = def->asBranchSeatDefinition();
        if (seat)
        {
          const BoundaryBranchSeatPlanEntry *plan = boundary ? boundary->branchSeatPlan(def) : 0;
          assert(plan && plan->condition && "conditional seat requires a captured Boundary plan");
          if (!plan || !plan->condition)
          {
            NodeMaterializationResult missingPlan = {0, false};
            return missingPlan;
          }
          const bool condition = plan->condition->get();
          loka::app::FragmentDefinition emptyBranch;
          NodeDefinitionBase *branchDefinition =
              plan->materializedBranchDefinition(condition, emptyBranch);
          NodeMaterializationResult active = createNodeRecursive(branchDefinition,
                                                                 autoIdCounter,
                                                                 boundary,
                                                                 runtimeParent);
          if (active.root)
          {
            boundary->registerMaterializedBranchSeat(*plan,
                                                     runtimeParent,
                                                     active.root,
                                                     condition);
          }
          return active;
        }

        Node *node = def->create();
        if (!node)
        {
          NodeMaterializationResult refused = {0, true};
          return refused;
        }
        assignNodeTestId(node, def, autoIdCounter);

        NodeMaterializationResult result = {node, false};

        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();

        if (nestableDef && nestableNode)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            NodeMaterializationResult childResult = createNodeRecursive(child,
                                                                        autoIdCounter,
                                                                        boundary,
                                                                        node);
            result.allocationFailed = result.allocationFailed || childResult.allocationFailed;
            if (childResult.root)
            {
              nestableNode->addChild(childResult.root);
            }
            child = child->nextInComposition;
          }
        }

        return result;
      }

      Node *NodeComposition::createNodeTree() const
      {
        assert(context_ && context_->boundary() &&
               "NodeComposition::createNodeTree requires BoundaryNode context");
        NodeMaterializationResult result =
            this->createNodeFromDefinitionResult(this->root());
        if (result.allocationFailed)
        {
          context_->boundary()->noteComposeAllocationFailure();
        }
        return result.root;
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
          for (NodeDefinitionBase *child = nestable->childrenHead(); child; child = child->nextInComposition)
          {
            assignDefinitionSeatSlots(child, nextSlot);
          }
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
          NodeDefinitionBase *root) const
      {
        if (!root)
        {
          NodeMaterializationResult empty = {0, false};
          return empty;
        }

        // Try to use arena if boundary is available
        if (context_)
        {
          BoundaryNode *bnd = context_->boundary();
          if (bnd)
          {
            NodeArena *arena = bnd->nodeArena();
            if (!arena->hasCapacity())
            {
              // An arena reservation refusal is storage-strategy degradation,
              // not a logical materialization failure. Only a refusal to
              // materialize at BOTH doors — the arena and the final heap door —
              // becomes a compose failure (#132 ruling 3).
              size_t totalSize = calculateTotalNodeSize(root, bnd);
              arena->reserve(totalSize);
            }
            long autoIdCounter = 1;
            return createNodeWithArena(root, arena, autoIdCounter, bnd, bnd);
          }
        }

        // Fallback without arena. The boundary here is always null (the
        // with-context arena path above returns whenever context_->boundary()
        // is non-null), so this contextless path never touches a boundary's
        // seat/arena state — the branch-seat plan lookup and materialized-seat
        // registration in createNodeRecursive stay disabled exactly as on main.
        // The completed result carries the allocation white flag instead
        // (#132 ruling 3), without a boundary being involved.
        long autoIdCounter = 1;
        BoundaryNode *boundary = context_ ? context_->boundary() : 0;
        return createNodeRecursive(root, autoIdCounter, boundary, boundary);
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
