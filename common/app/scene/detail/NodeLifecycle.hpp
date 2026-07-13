#ifndef LOKA_APP_SCENE_DETAIL_NODE_LIFECYCLE_HPP
#define LOKA_APP_SCENE_DETAIL_NODE_LIFECYCLE_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {
        inline void notifyNodeAttachedRecursive(Node *node)
        {
          if (!node)
          {
            return;
          }
          node->onCompositionAttached();
          if (node->getContext())
          {
            node->getContext()->onNodeAttached();
          }
          INestable *nestable = node->asNestable();
          if (!nestable)
          {
            return;
          }
          for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
          {
            notifyNodeAttachedRecursive(child);
          }
        }

        inline void notifyNodeDetachedRecursive(Node *node)
        {
          if (!node)
          {
            return;
          }
          node->onCompositionDetached();
          if (node->getContext())
          {
            node->getContext()->onNodeDetached();
          }
          INestable *nestable = node->asNestable();
          if (!nestable)
          {
            return;
          }
          for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
          {
            notifyNodeDetachedRecursive(child);
          }
        }
      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_APP_SCENE_DETAIL_NODE_LIFECYCLE_HPP
