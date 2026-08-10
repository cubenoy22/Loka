#ifndef LOKA_APP2_NODES_NESTABLE_BOX_HPP
#define LOKA_APP2_NODES_NESTABLE_BOX_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct BoxTypeTag
    {
    };

    class BoxNode;

    struct BoxProps : public scene::NodePropsBase<BoxProps>
    {
      typedef BoxTypeTag TypeTag;
      typedef BoxNode NodeType;
      int padding;
      short width;
      short height;
      BoxProps()
          : padding(0),
            width(0),
            height(0)
      {
      }
      int hash() const
      {
        return padding + width + height;
      }
      BoxProps &setPadding(int value)
      {
        padding = value;
        return *this;
      }
      /** Declares the fixed outer area owned by this container. */
      BoxProps &setSize(short fixedWidth, short fixedHeight)
      {
        width = fixedWidth;
        height = fixedHeight;
        return *this;
      }
      /** True when this container owns an explicit outer layout extent. */
      bool hasFixedSize() const
      {
        return width > 0 && height > 0;
      }
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const BoxProps &other = static_cast<const BoxProps &>(rhs);
        if (padding != other.padding)
          return padding < other.padding;
        if (width != other.width)
          return width < other.width;
        return height < other.height;
      }
    };

    class BoxNode : public scene::NestableNode
    {
    public:
      typedef BoxTypeTag TypeTag;
      BoxProps props;
      BoxNode(const BoxProps &p)
          : scene::NestableNode(),
            props(p)
      {
      }
      virtual scene::NodeKind kind() const
      {
        return scene::NODE_KIND_BOX;
      }
      virtual const void *nodeTypeKey() const
      {
        return scene::NodeTypeToken<BoxNode>();
      }
      virtual BoxNode *asBoxNode()
      {
        return this;
      }
    };

    struct BoxDefinition : public scene::NestableNodeDefinition<BoxProps, BoxNode, BoxDefinition>,
                           public scene::TestIdDslMixin<BoxDefinition>
    {
      typedef scene::NestableNodeDefinition<BoxProps, BoxNode, BoxDefinition> BaseType;
      using BaseType::operator<<;

      BoxDefinition()
          : BaseType()
      {
      }
      BoxDefinition(const BoxProps &p)
          : BaseType(p)
      {
      }
      BoxDefinition(const BoxDefinition &other)
          : BaseType(other)
      {
      }
      BoxDefinition &padding(int value)
      {
        this->props.setPadding(value);
        return *this;
      }
      /** Declares the fixed outer area owned by this container. */
      BoxDefinition &size(short width, short height)
      {
        this->props.setSize(width, height);
        return *this;
      }
    };

    typedef BoxDefinition Box;

    inline BoxNode *createNode(const BoxProps &props)
    {
      return new BoxNode(props);
    }

  } // namespace app
} // namespace loka

#endif // LOKA_APP2_NODES_NESTABLE_BOX_HPP
