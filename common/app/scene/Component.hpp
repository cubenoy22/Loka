#ifndef LOKA_CORE2_SCENE_COMPONENT_HPP
#define LOKA_CORE2_SCENE_COMPONENT_HPP

#include <cassert>
#include "app/Fragment.hpp"
#include "app/scene/NodeComposition.hpp"

namespace loka
{
  namespace app
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
          NodeComposition::ChildComposition child(composition, parent);
          component_->composeNode(child);
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

      template <class ComponentT>
      inline ::loka::app::FragmentDefinition LightComponent(ComponentT *component)
      {
        assert(component && "LightComponent requires component instance");
        NodeComposition *composition = NodeComposition::current();
        assert(composition && "LightComponent requires active NodeComposition");
        component->attachNode(*composition);
        ::loka::app::FragmentDefinition fragment;
        NodeComposition::ChildComposition child(*composition, fragment);
        component->composeNode(child);
        return fragment;
      }

      template <class ComponentT>
      inline ::loka::app::FragmentDefinition LightComponent(ComponentT &component)
      {
        return LightComponent(&component);
      }
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_COMPONENT_HPP
