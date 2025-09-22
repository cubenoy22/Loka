#ifndef DECLARA_APP2_BOX_HPP
#define DECLARA_APP2_BOX_HPP

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
    struct BoxProps : public core::scene::NodePropsBase<BoxProps>
    {
      // レイアウト用プロパティ（例: direction, spacing など）
      // 今回は最小限で空のまま
      // int direction;
      // int spacing;
      typedef BoxTypeTag TypeTag;
      BoxProps() {}
      int hash() const { return 0; }
      bool operator<(const core::scene::PropsBase &rhs) const { return false; }
    };

    class BoxNode : public core::scene::Node
    {
    public:
      typedef BoxTypeTag TypeTag;
      BoxProps props;
      BoxNode(const BoxProps &p) : props(p) {}
      // レイアウトロジックは今後追加
    };

    struct BoxDefinition : public core::scene::NodeDefinition<BoxProps, BoxNode>, public core::scene::INestableDefinition
    {
      std::vector<core::scene::NodeDefinitionBase *> children_;
      BoxDefinition() : NodeDefinition() {}
      BoxDefinition(const BoxProps &p) : NodeDefinition(p) {}
      virtual void addChild(core::scene::NodeDefinitionBase *child)
      {
        children_.push_back(child);
      }
      virtual const std::vector<core::scene::NodeDefinitionBase *> &getChildren() const
      {
        return children_;
      }
    };
    // DSL向け短縮名
    typedef BoxDefinition Box;

    inline BoxNode *createNode(const BoxProps &props)
    {
      return new BoxNode(props);
    }

  } // namespace app
} // namespace declara

#endif // DECLARA_APP2_BOX_HPP
