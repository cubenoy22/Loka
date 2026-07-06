#ifndef LOKA_APP2_NODES_NESTABLE_ZSTACK_HPP
#define LOKA_APP2_NODES_NESTABLE_ZSTACK_HPP

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
      ZStackNode(const ZStackProps &p)
          : scene::NestableNode(),
            props(p)
      {
      }
      virtual scene::NodeKind kind() const
      {
        return scene::NODE_KIND_ZSTACK;
      }
      virtual ZStackNode *asZStackNode()
      {
        return this;
      }
    };

    struct ZStackDefinition : public scene::NestableNodeDefinition<ZStackProps, ZStackNode, ZStackDefinition>,
                              public scene::TestIdDslMixin<ZStackDefinition>
    {
      typedef scene::NestableNodeDefinition<ZStackProps, ZStackNode, ZStackDefinition> BaseType;
      using BaseType::operator<<;
      ZStackDefinition()
          : BaseType()
      {
      }
      ZStackDefinition(const ZStackProps &p)
          : BaseType(p)
      {
      }
      ZStackDefinition(const ZStackDefinition &other)
          : BaseType(other)
      {
      }
    };

    typedef ZStackDefinition ZStack;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_NODES_NESTABLE_ZSTACK_HPP
