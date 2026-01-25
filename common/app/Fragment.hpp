#ifndef LOKA_APP2_FRAGMENT_HPP
#define LOKA_APP2_FRAGMENT_HPP

#include "core2/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct FragmentTypeTag
    {
    };

    class FragmentNode;

    struct FragmentProps : public core::scene::NodePropsBase<FragmentProps>
    {
      typedef FragmentTypeTag TypeTag;
      typedef FragmentNode NodeType;
      FragmentProps() {}
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        return false; // no fields to compare
      }
    };

    class FragmentNode : public core::scene::NestableNode
    {
    public:
      typedef FragmentTypeTag TypeTag;
      FragmentProps props;
      FragmentNode(const FragmentProps &p) : props(p) {}
    };

    struct FragmentDefinition : public core::scene::NodeDefinition<FragmentProps, FragmentNode>, public core::scene::NestableDefinitionBase
    {
      typedef core::scene::NodeDefinition<FragmentProps, FragmentNode> BaseType;
      FragmentDefinition() : BaseType(), core::scene::NestableDefinitionBase() {}
      FragmentDefinition(const FragmentProps &p) : BaseType(p), core::scene::NestableDefinitionBase() {}
      FragmentDefinition(const FragmentDefinition &other) : BaseType(other), core::scene::NestableDefinitionBase(other) {}
      FragmentDefinition &operator=(const FragmentDefinition &other)
      {
        if (this != &other)
        {
          BaseType::operator=(other);
          core::scene::NestableDefinitionBase::operator=(other);
        }
        return *this;
      }
      virtual core::scene::NodeDefinitionBase *clone() const
      {
        return new FragmentDefinition(*this);
      }
      virtual core::scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const core::scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
    };

    typedef FragmentDefinition Fragment;
    typedef FragmentDefinition F;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_FRAGMENT_HPP
