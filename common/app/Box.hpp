#ifndef LOKA_APP2_BOX_HPP
#define LOKA_APP2_BOX_HPP

#include <string>
#include "core2/scene/Node.hpp"
#include <vector>

namespace declara
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
      BoxProps() {}
      int hash() const { return 0; }
      bool operator<(const core::scene::PropsBase &rhs) const { return false; }
    };

    class BoxNode : public core::scene::NestableNode
    {
    public:
      typedef BoxTypeTag TypeTag;
      BoxProps props;
      BoxNode(const BoxProps &p) : core::scene::NestableNode(), props(p) {}
      // レイアウトロジックは今後追加
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
    };
    // DSL向け短縮名
    typedef BoxDefinition Box;

    inline BoxNode *createNode(const BoxProps &props)
    {
      return new BoxNode(props);
    }

  } // namespace app
} // namespace declara

#endif // LOKA_APP2_BOX_HPP
