#ifndef LOKA_LAZY_SCOPE_NODE_HPP
#define LOKA_LAZY_SCOPE_NODE_HPP
#include "app/scene/boundary/GenerationRoot.hpp"
namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <class K, class NodeT> class LazyScopeDefinition;
      /** Generation owner whose own constructor states and declaration bindings
          connect in the candidate window before declareScope. */
      class LazyScopeNode : public GenerationRoot
      {
      public:
        virtual const void *nodeTypeKey() const
        {
          return NodeTypeToken<LazyScopeNode>();
        }
        virtual void declareScope(NodeComposition &composition) = 0;

      private:
        template <class K, class NodeT> friend class LazyScopeDefinition;
        void prepareScope(ComponentContext &context, NodeComposition &composition)
        {
          this->stateOwner_.attachStateOwner(context.boundary(), context.stateOwner());
          ContextScope scope(this, &context);
          this->nodeStateOwner_ = &this->stateOwner_;
          this->connectNodeStateRegistrations();
          if (this->scopeStatus() != LAZY_SCOPE_READY)
            return;
          this->beginDeclaringWindow(context);
          if (this->scopeStatus() != LAZY_SCOPE_READY)
            return;
          assert(context.owner() == this && context.stateOwner() == &this->stateOwner_);
          composition.setContext(&context);
          {
            NodeComposition::CompositionScope window(composition);
            this->declareScope(composition);
          }
          composition.setContext(0);
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
