#ifndef LOKA_APP2_FRAGMENT_HPP
#define LOKA_APP2_FRAGMENT_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct FragmentTypeTag
    {
    };

    class FragmentNode;

    struct FragmentProps : public scene::NodePropsBase<FragmentProps>
    {
      typedef FragmentTypeTag TypeTag;
      typedef FragmentNode NodeType;
      FragmentProps() {}
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        return false; // no fields to compare
      }
    };

    class FragmentNode : public scene::NestableNode
    {
    public:
      typedef FragmentTypeTag TypeTag;
      FragmentProps props;
      FragmentNode(const FragmentProps &p) : props(p) {}
      virtual void render(scene::IPlatformController *controller)
      {
        scene::Node *child = this->childrenHead();
        while (child)
        {
          child->render(controller);
          child = child->nextInComposition;
        }
      }
      virtual short layout(scene::IPlatformController *controller, scene::LayoutState &state)
      {
        short result = 0;
        scene::Node *child = this->childrenHead();
        while (child)
        {
          result = child->layout(controller, state);
          child = child->nextInComposition;
        }
        return result;
      }
    };

    struct FragmentDefinition : public scene::NodeDefinition<FragmentProps, FragmentNode>, public scene::NestableDefinitionBase, public scene::TestIdDslMixin<FragmentDefinition>
    {
      typedef scene::NodeDefinition<FragmentProps, FragmentNode> BaseType;
      FragmentDefinition() : BaseType(), scene::NestableDefinitionBase() {}
      FragmentDefinition(const FragmentProps &p) : BaseType(p), scene::NestableDefinitionBase() {}
      FragmentDefinition(const FragmentDefinition &other) : BaseType(other), scene::NestableDefinitionBase(other) {}
      FragmentDefinition &operator=(const FragmentDefinition &other)
      {
        if (this != &other)
        {
          BaseType::operator=(other);
          scene::NestableDefinitionBase::operator=(other);
        }
        return *this;
      }
      virtual scene::NodeDefinitionBase *clone() const
      {
        return new FragmentDefinition(*this);
      }
      virtual scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
    };

    typedef FragmentDefinition Fragment;
    typedef FragmentDefinition F;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_FRAGMENT_HPP
