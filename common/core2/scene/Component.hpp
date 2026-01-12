#ifndef LOKA_CORE2_SCENE_COMPONENT_HPP
#define LOKA_CORE2_SCENE_COMPONENT_HPP

#include <cassert>
#include "core2/scene/NodeComposition.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      struct ComponentDefinitionBase
      {
        virtual ~ComponentDefinitionBase() {}
        virtual void composeInto(NodeComposition &composition, INestableDefinition &parent) = 0;
      };

      template <class ComponentT>
      struct ComponentDefinition : public ComponentDefinitionBase
      {
        explicit ComponentDefinition(ComponentT *component) : component_(component) {}
        virtual void composeInto(NodeComposition &composition, INestableDefinition &parent)
        {
          assert(component_ && "ComponentDefinition requires component instance");
          component_->composeInto(composition, parent);
        }
        ComponentT *component_;
      };

      template <class ComponentT>
      struct ComponentTag
      {
        ComponentDefinition<ComponentT> operator()(ComponentT &component) const
        {
          return ComponentDefinition<ComponentT>(&component);
        }
      };

      inline INestableDefinition &INestableDefinition::operator<<(ComponentDefinitionBase &component)
      {
        NodeComposition *composition = NodeComposition::current();
        assert(composition && "ComponentDefinition requires active NodeComposition");
        component.composeInto(*composition, *this);
        return *this;
      }

      inline INestableDefinition &INestableDefinition::operator<<(const ComponentDefinitionBase &component)
      {
        ComponentDefinitionBase &mutableComponent = const_cast<ComponentDefinitionBase &>(component);
        return (*this) << mutableComponent;
      }
    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_COMPONENT_HPP
