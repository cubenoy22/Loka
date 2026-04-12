#ifndef LOKA_APP2_BOX_HPP
#define LOKA_APP2_BOX_HPP

#include <string>
#include "app/scene/Node.hpp"
#include <vector>

namespace loka
{
  namespace app
  {
    struct BoxTypeTag
    {
    };

    // Forward declaration
    class BoxNode;

    struct BoxProps : public scene::NodePropsBase<BoxProps>
    {
      // レイアウト用プロパティ（例: direction, spacing など）
      // 今回は最小限で空のまま
      // int direction;
      // int spacing;
      typedef BoxTypeTag TypeTag;
      typedef BoxNode NodeType;
      int padding;
      BoxProps() : padding(0) {}
      int hash() const { return padding; }
      BoxProps &setPadding(int value)
      {
        padding = value;
        return *this;
      }
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const BoxProps &other = static_cast<const BoxProps &>(rhs);
        return padding < other.padding;
      }
    };

    class BoxNode : public scene::NestableNode
    {
    public:
      typedef BoxTypeTag TypeTag;
      BoxProps props;
      BoxNode(const BoxProps &p) : scene::NestableNode(), props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_BOX; }
      virtual BoxNode *asBoxNode() { return this; }
    };

    struct BoxDefinition : public scene::NodeDefinition<BoxProps, BoxNode>, public scene::NestableDefinitionBase, public scene::NestableDslMixin<BoxDefinition>, public scene::TestIdDslMixin<BoxDefinition>
    {
      typedef scene::NodeDefinition<BoxProps, BoxNode> BaseType;
      using scene::NestableDslMixin<BoxDefinition>::operator<<;

      BoxDefinition() : BaseType(), NestableDefinitionBase() {}
      BoxDefinition(const BoxProps &p) : BaseType(p), NestableDefinitionBase() {}
      BoxDefinition(const BoxDefinition &other) : BaseType(other), NestableDefinitionBase(other) {}
      BoxDefinition &operator=(const BoxDefinition &other)
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
        return new BoxDefinition(*this);
      }
      virtual scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
      BoxDefinition &padding(int value)
      {
        this->props.setPadding(value);
        return *this;
      }
    };
    // DSL向け短縮名
    typedef BoxDefinition Box;

    inline BoxNode *createNode(const BoxProps &props)
    {
      return new BoxNode(props);
    }

  } // namespace app
} // namespace loka

#endif // LOKA_APP2_BOX_HPP
