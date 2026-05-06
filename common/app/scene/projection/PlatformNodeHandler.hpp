#ifndef LOKA_CORE2_SCENE_PROJECTION_PLATFORM_NODE_HANDLER_HPP
#define LOKA_CORE2_SCENE_PROJECTION_PLATFORM_NODE_HANDLER_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class IPlatformController;

      class IPlatformNodeHandler
      {
      public:
        virtual ~IPlatformNodeHandler() {}

        virtual const void *nodeTypeKey() const = 0;
        virtual NodeContext *ensureContext(Node *node, IPlatformController *controller, const LayoutState &state) = 0;
      };

      class PlatformNodeHandlerRegistry
      {
      public:
        PlatformNodeHandlerRegistry()
            : head_(0)
        {
        }
        ~PlatformNodeHandlerRegistry()
        {
          HandlerEntry *entry = this->head_;
          while (entry)
          {
            HandlerEntry *next = entry->next_;
            delete entry;
            entry = next;
          }
          this->head_ = 0;
        }

        // Non-owning registry: built-in handlers are commonly static objects and
        // external callers may register stack/static handlers through
        // IPlatformController::registerNodeHandler(...). The registry owns only
        // its entry list, not the handler instances themselves.
        bool registerHandler(IPlatformNodeHandler *handler)
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
              // Replacement is pointer-only on purpose; lifecycle remains with
              // the caller/handler owner.
              existing->handler_ = handler;
              return true;
            }
            existing = existing->next_;
          }

          HandlerEntry *entry = new HandlerEntry(handler->nodeTypeKey(), handler);
          if (!entry)
          {
            return false;
          }
          entry->next_ = this->head_;
          this->head_ = entry;
          return true;
        }

        IPlatformNodeHandler *find(const Node *node) const
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
          HandlerEntry(const void *nodeTypeKey, IPlatformNodeHandler *handler)
              : nodeTypeKey_(nodeTypeKey),
                handler_(handler),
                next_(0)
          {
          }

          const void *nodeTypeKey_;
          IPlatformNodeHandler *handler_;
          HandlerEntry *next_;
        };

        HandlerEntry *head_;

        PlatformNodeHandlerRegistry(const PlatformNodeHandlerRegistry &);
        PlatformNodeHandlerRegistry &operator=(const PlatformNodeHandlerRegistry &);
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_PLATFORM_NODE_HANDLER_HPP
