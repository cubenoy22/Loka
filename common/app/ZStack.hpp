#ifndef LOKA_APP2_ZSTACK_HPP
#define LOKA_APP2_ZSTACK_HPP

#include "core2/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct ZStackTypeTag
    {
    };

    class ZStackNode;

    struct ZStackProps : public core::scene::NodePropsBase<ZStackProps>
    {
      typedef ZStackTypeTag TypeTag;
      typedef ZStackNode NodeType;
      ZStackProps() {}
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        return false;
      }
    };

    class ZStackNode : public core::scene::NestableNode
    {
    public:
      typedef ZStackTypeTag TypeTag;
      ZStackProps props;
      ZStackNode(const ZStackProps &p) : core::scene::NestableNode(), props(p) {}
      virtual core::scene::NodeKind kind() const { return core::scene::NODE_KIND_ZSTACK; }
      virtual ZStackNode *asZStackNode() { return this; }
    };

    struct ZStackDefinition : public core::scene::NodeDefinition<ZStackProps, ZStackNode>, public core::scene::NestableDefinitionBase
    {
      typedef core::scene::NodeDefinition<ZStackProps, ZStackNode> BaseType;
      ZStackDefinition() : BaseType(), core::scene::NestableDefinitionBase() {}
      ZStackDefinition(const ZStackProps &p) : BaseType(p), core::scene::NestableDefinitionBase() {}
      ZStackDefinition(const ZStackDefinition &other) : BaseType(other), core::scene::NestableDefinitionBase(other) {}
      ZStackDefinition &operator=(const ZStackDefinition &other)
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
        return new ZStackDefinition(*this);
      }
      virtual core::scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const core::scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
    };

    typedef ZStackDefinition ZStack;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_ZSTACK_HPP
