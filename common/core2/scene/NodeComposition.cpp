#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/Node.hpp"
#include "core2/scene/Scene.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "app/Window.hpp"
#include <new>

namespace loka
{
  namespace core
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

      // 宣言のみ。実際の型はNode.hppで定義されるINestableなどから取得
      struct INestableDefinition;
      struct INestable;

      // Pass 1: Calculate total size needed for all nodes
      static size_t calculateTotalNodeSize(NodeDefinitionBase *def)
      {
        if (!def)
        {
          return 0;
        }
        INestableDefinition *nestableDef = def->asNestableDefinition();
        // Add size with alignment padding (worst case)
        size_t align = NodeArena::normalizeAlign(def->nodeAlign());
        size_t total = def->nodeSize() + align;

        if (nestableDef)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            total += calculateTotalNodeSize(child);
            child = child->nextInComposition;
          }
        }
        return total;
      }

      // Pass 2: Create nodes using arena
      static Node *createNodeWithArena(NodeDefinitionBase *def, NodeArena *arena)
      {
        if (!def)
        {
          return 0;
        }

        // Allocate from arena
        size_t nodeSize = def->nodeSize();
        size_t nodeAlign = def->nodeAlign();
        void *mem = arena->allocate(nodeSize, nodeAlign);
        Node *node;
        if (mem)
        {
          node = def->createInPlace(mem);
          node->setArenaAllocated(true);
          arena->registerNode(node);
        }
        else
        {
          // Fallback to regular allocation
          node = def->create();
        }

        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();

        if (nestableDef && nestableNode)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            Node *childNode = createNodeWithArena(child, arena);
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
      static Node *createNodeRecursive(NodeDefinitionBase *def)
      {
        if (!def)
        {
          return 0;
        }

        Node *node = def->create();

        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();

        if (nestableDef && nestableNode)
        {
          NodeDefinitionBase *child = nestableDef->childrenHead();
          while (child)
          {
            Node *childNode = createNodeRecursive(child);
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
              size_t totalSize = calculateTotalNodeSize(root);
              arena->reserve(totalSize);
            }
            return createNodeWithArena(root, arena);
          }
        }

        // Fallback without arena
        return createNodeRecursive(root);
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

    }
  }
}
