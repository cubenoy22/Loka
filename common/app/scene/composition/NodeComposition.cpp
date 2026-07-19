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

      static Node *createNodeWithArena(NodeDefinitionBase *def,
                                       NodeArena *arena,
                                       long &autoIdCounter,
                                       BoundaryNode *boundary,
                                       Node *runtimeParent)
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
          const bool condition = plan->condition->get();
          loka::app::FragmentDefinition emptyBranch;
          NodeDefinitionBase *branchDefinition =
              plan->materializedBranchDefinition(condition, emptyBranch);
          Node *active = createNodeWithArena(branchDefinition,
                                             arena,
                                             autoIdCounter,
                                             boundary,
                                             runtimeParent);
          if (active)
          {
            boundary->registerMaterializedBranchSeat(*plan,
                                                     runtimeParent,
                                                     active,
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
        assignNodeTestId(node, def, autoIdCounter);

        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();

        if (nestableDef && nestableNode)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            Node *childNode = createNodeWithArena(child,
                                                  arena,
                                                  autoIdCounter,
                                                  boundary,
                                                  node);
            if (childNode)
            {
              nestableNode->addChild(childNode);
            }
            child = child->nextInComposition;
          }
        }

        return node;
      }

      // Fallback: create without arena
      static Node *createNodeRecursive(NodeDefinitionBase *def,
                                       long &autoIdCounter,
                                       BoundaryNode *boundary,
                                       Node *runtimeParent)
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
          const bool condition = plan->condition->get();
          loka::app::FragmentDefinition emptyBranch;
          NodeDefinitionBase *branchDefinition =
              plan->materializedBranchDefinition(condition, emptyBranch);
          Node *active = createNodeRecursive(branchDefinition,
                                             autoIdCounter,
                                             boundary,
                                             runtimeParent);
          if (active)
          {
            boundary->registerMaterializedBranchSeat(*plan,
                                                     runtimeParent,
                                                     active,
                                                     condition);
          }
          return active;
        }

        Node *node = def->create();
        assignNodeTestId(node, def, autoIdCounter);

        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();

        if (nestableDef && nestableNode)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            Node *childNode = createNodeRecursive(child,
                                                  autoIdCounter,
                                                  boundary,
                                                  node);
            if (childNode)
            {
              nestableNode->addChild(childNode);
            }
            child = child->nextInComposition;
          }
        }

        return node;
      }

      Node *NodeComposition::createNodeTree() const
      {
        NodeDefinitionBase *root = this->root();
        if (!root)
        {
          return 0;
        }

        return this->createNodeFromDefinition(root);
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

      Node *NodeComposition::createNodeFromDefinition(NodeDefinitionBase *root) const
      {
        if (!root)
        {
          return 0;
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
              // Calculate total size and reserve
              size_t totalSize = calculateTotalNodeSize(root, bnd);
              arena->reserve(totalSize);
            }
            long autoIdCounter = 1;
            return createNodeWithArena(root, arena, autoIdCounter, bnd, bnd);
          }
        }

        // Fallback without arena
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
