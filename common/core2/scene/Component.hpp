#ifndef LOKA_CORE2_SCENE_COMPONENT_HPP
#define LOKA_CORE2_SCENE_COMPONENT_HPP

#include <cassert>
#include "core2/scene/NodeComposition.hpp"

namespace loka
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

      struct LightComponentDefinitionBase
      {
        virtual ~LightComponentDefinitionBase() {}
        virtual void attachInto(NodeComposition &composition) = 0;
        virtual void composeInto(NodeComposition &composition, INestableDefinition &parent) = 0;
      };

      template <class ComponentT>
      struct LightComponentDefinition : public LightComponentDefinitionBase
      {
        explicit LightComponentDefinition(ComponentT *component) : component_(component) {}
        virtual void attachInto(NodeComposition &composition)
        {
          assert(component_ && "LightComponentDefinition requires component instance");
          component_->attachNode(composition);
        }
        virtual void composeInto(NodeComposition &composition, INestableDefinition &parent)
        {
          assert(component_ && "LightComponentDefinition requires component instance");
          NodeComposition::ChildComposition child(composition, parent);
          component_->composeNode(child);
        }
        ComponentT *component_;
      };

      template <class ComponentT>
      inline LightComponentDefinition<ComponentT> LightComponent(ComponentT *component)
      {
        return LightComponentDefinition<ComponentT>(component);
      }

      template <class ComponentT>
      inline LightComponentDefinition<ComponentT> LightComponent(ComponentT &component)
      {
        return LightComponentDefinition<ComponentT>(&component);
      }

      template <class ComponentT>
      inline LightComponentDefinition<ComponentT> LC(ComponentT *component)
      {
        return LightComponent(component);
      }

      template <class ComponentT>
      inline LightComponentDefinition<ComponentT> LC(ComponentT &component)
      {
        return LightComponent(component);
      }

      inline INestableDefinition &INestableDefinition::operator<<(LightComponentDefinitionBase &component)
      {
        NodeComposition *composition = NodeComposition::current();
        assert(composition && "LightComponentDefinition requires active NodeComposition");
        component.attachInto(*composition);
        component.composeInto(*composition, *this);
        return *this;
      }

      inline INestableDefinition &INestableDefinition::operator<<(const LightComponentDefinitionBase &component)
      {
        LightComponentDefinitionBase &mutableComponent = const_cast<LightComponentDefinitionBase &>(component);
        return (*this) << mutableComponent;
      }
    } // namespace scene
  } // namespace core
} // namespace loka

#endif // LOKA_CORE2_SCENE_COMPONENT_HPP
