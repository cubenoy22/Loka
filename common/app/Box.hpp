#ifndef LOKA_APP2_BOX_HPP
#define LOKA_APP2_BOX_HPP

#include <string>
#include "core2/scene/Node.hpp"
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

    struct BoxProps : public core::scene::NodePropsBase<BoxProps>
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
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const BoxProps &other = static_cast<const BoxProps &>(rhs);
        return padding < other.padding;
      }
    };

    class BoxNode : public core::scene::NestableNode
    {
    public:
      typedef BoxTypeTag TypeTag;
      BoxProps props;
      BoxNode(const BoxProps &p) : core::scene::NestableNode(), props(p) {}
      virtual core::scene::NodeKind kind() const { return core::scene::NODE_KIND_BOX; }
      virtual BoxNode *asBoxNode() { return this; }
    };

    struct BoxDefinition : public core::scene::NodeDefinition<BoxProps, BoxNode>, public core::scene::NestableDefinitionBase
    {
      typedef core::scene::NodeDefinition<BoxProps, BoxNode> BaseType;

      BoxDefinition() : BaseType(), NestableDefinitionBase() {}
      BoxDefinition(const BoxProps &p) : BaseType(p), NestableDefinitionBase() {}
      BoxDefinition(const BoxDefinition &other) : BaseType(other), NestableDefinitionBase(other) {}
      BoxDefinition &operator=(const BoxDefinition &other)
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
        return new BoxDefinition(*this);
      }
      virtual core::scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const core::scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
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
