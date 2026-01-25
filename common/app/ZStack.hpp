#ifndef LOKA_APP2_ZSTACK_HPP
#define LOKA_APP2_ZSTACK_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct ZStackTypeTag
    {
    };

    class ZStackNode;

    struct ZStackProps : public scene::NodePropsBase<ZStackProps>
    {
      typedef ZStackTypeTag TypeTag;
      typedef ZStackNode NodeType;
      ZStackProps() {}
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        return false;
      }
    };

    class ZStackNode : public scene::NestableNode
    {
    public:
      typedef ZStackTypeTag TypeTag;
      ZStackProps props;
      ZStackNode(const ZStackProps &p) : scene::NestableNode(), props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_ZSTACK; }
      virtual ZStackNode *asZStackNode() { return this; }
    };

    struct ZStackDefinition : public scene::NodeDefinition<ZStackProps, ZStackNode>, public scene::NestableDefinitionBase
    {
      typedef scene::NodeDefinition<ZStackProps, ZStackNode> BaseType;
      ZStackDefinition() : BaseType(), scene::NestableDefinitionBase() {}
      ZStackDefinition(const ZStackProps &p) : BaseType(p), scene::NestableDefinitionBase() {}
      ZStackDefinition(const ZStackDefinition &other) : BaseType(other), scene::NestableDefinitionBase(other) {}
      ZStackDefinition &operator=(const ZStackDefinition &other)
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
        return new ZStackDefinition(*this);
      }
      virtual scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
    };

    typedef ZStackDefinition ZStack;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_ZSTACK_HPP
