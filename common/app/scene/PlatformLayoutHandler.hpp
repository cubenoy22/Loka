#ifndef LOKA_CORE2_SCENE_PLATFORM_LAYOUT_HANDLER_HPP
#define LOKA_CORE2_SCENE_PLATFORM_LAYOUT_HANDLER_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class IPlatformLayoutTraversal
      {
      public:
        virtual ~IPlatformLayoutTraversal() {}
        virtual int layoutChild(Node *child, const LayoutState &state) = 0;
        virtual void setLayoutResultY(short y) = 0;
        virtual short layoutResultY() const = 0;
      };

      class IPlatformLayoutHandler
      {
      public:
        virtual ~IPlatformLayoutHandler() {}

        virtual const void *nodeTypeKey() const = 0;
        virtual int layoutNode(Node *node,
                               const LayoutState &state,
                               IPlatformLayoutTraversal *traversal) = 0;
      };

      class PlatformLayoutHandlerRegistry
      {
      public:
        PlatformLayoutHandlerRegistry() : head_(0) {}
        ~PlatformLayoutHandlerRegistry()
        {
          HandlerEntry *entry = this->head_;
          while (entry)
          {
            HandlerEntry *next = entry->next_;
            delete entry->handler_;
            delete entry;
            entry = next;
          }
          this->head_ = 0;
        }

        bool registerHandler(IPlatformLayoutHandler *handler)
        {
          if (!handler || !handler->nodeTypeKey())
          {
            return false;
          }

          HandlerEntry *existing = this->head_;
          while (existing)
          {
            if (existing->nodeTypeKey_ == handler->nodeTypeKey())
            {
              if (existing->handler_ != handler)
              {
                delete existing->handler_;
              }
              existing->handler_ = handler;
              return true;
            }
            existing = existing->next_;
          }

          HandlerEntry *entry = new HandlerEntry(handler->nodeTypeKey(), handler);
          if (!entry)
          {
            delete handler;
            return false;
          }
          entry->next_ = this->head_;
          this->head_ = entry;
          return true;
        }

        IPlatformLayoutHandler *find(const Node *node) const
        {
          if (!node)
          {
            return 0;
          }
          const void *typeKey = node->nodeTypeKey();
          if (!typeKey)
          {
            return 0;
          }

          HandlerEntry *entry = this->head_;
          while (entry)
          {
            if (entry->nodeTypeKey_ == typeKey)
            {
              return entry->handler_;
            }
            entry = entry->next_;
          }
          return 0;
        }

      private:
        struct HandlerEntry
        {
          HandlerEntry(const void *nodeTypeKey, IPlatformLayoutHandler *handler)
              : nodeTypeKey_(nodeTypeKey), handler_(handler), next_(0) {}

          const void *nodeTypeKey_;
          IPlatformLayoutHandler *handler_;
          HandlerEntry *next_;
        };

        HandlerEntry *head_;

        PlatformLayoutHandlerRegistry(const PlatformLayoutHandlerRegistry &);
        PlatformLayoutHandlerRegistry &operator=(const PlatformLayoutHandlerRegistry &);
      };
    }
  }
}

#endif // LOKA_CORE2_SCENE_PLATFORM_LAYOUT_HANDLER_HPP
