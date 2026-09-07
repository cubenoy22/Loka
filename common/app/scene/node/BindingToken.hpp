#ifndef LOKA_APP_SCENE_NODE_BINDINGTOKEN_HPP
#define LOKA_APP_SCENE_NODE_BINDINGTOKEN_HPP

namespace loka
{
  namespace core { class EmitterState; }
  namespace app
  {
    namespace scene
    {
      class ComposableNode;

      /** Node-owned capability armed only while the kernel calls declareBindings.
          A borrowed pointer remains valid only for the owning node's lifetime;
          outside the declaration call its doors refuse to bind. */
      class BindingToken
      {
      public:
        template <class NodeT>
        void action(loka::core::EmitterState &emitter, NodeT *node, void (NodeT::*method)());
        template <class StateT, class NodeT>
        void watch(StateT &state, NodeT *node, void (NodeT::*method)(), bool callImmediately = false);

      private:
        friend class ComposableNode;
        BindingToken() : owner_(0) {}
        BindingToken(const BindingToken &);
        BindingToken &operator=(const BindingToken &);

        /** The kernel alone opens this scope; teardown always disarms it. */
        class DeclarationScope
        {
        public:
          DeclarationScope(BindingToken &token, ComposableNode &owner)
              : token_(token)
          {
            this->token_.owner_ = &owner;
          }
          ~DeclarationScope() { this->token_.owner_ = 0; }
        private:
          DeclarationScope(const DeclarationScope &);
          DeclarationScope &operator=(const DeclarationScope &);
          BindingToken &token_;
        };

        ComposableNode *owner_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

// Template forwarding needs the completed owner, whichever header is included first.
#include "app/scene/node/ComposableNode.hpp"

#endif // LOKA_APP_SCENE_NODE_BINDINGTOKEN_HPP
