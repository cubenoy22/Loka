#ifndef LOKA_PAINT_ENUMERATION_HPP
#define LOKA_PAINT_ENUMERATION_HPP
#include "app/scene/boundary/Boundary.hpp"
namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Synchronous borrowed residents. The common traversal never casts a context. */
      struct IPaintResidentVisitor
      {
        virtual ~IPaintResidentVisitor() {}
        virtual void visit(Node *resident, NodeContext *context, BoundaryNode *owner) = 0;
      };
      namespace paint_detail
      {
        inline void enumerate(Node *node, BoundaryNode *owner, IPaintResidentVisitor &visitor)
        {
          if (!node || node->lifecycleFact() != NODE_FACT_ATTACHED)
            return;
          if (node->asBoundary())
            owner = node->asBoundary();
          visitor.visit(node, node->getContext(), owner);
          INestable *nestable = node->asNestable();
          for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
            enumerate(child, owner, visitor);
        }
      } // namespace paint_detail
      /** Enters attached nested owners exactly once, never retained parked branches. */
      inline void enumerateAttachedResidents(BoundaryNode *root, IPaintResidentVisitor &visitor)
      {
        paint_detail::enumerate(root, root, visitor);
      }
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
