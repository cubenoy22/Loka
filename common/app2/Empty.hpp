#ifndef DECLARA_APP2_EMPTY_HPP
#define DECLARA_APP2_EMPTY_HPP

// Carbon/Toolbox流の空ノード定義
// Button/Boxと同じinterface/命名規則に準拠

#include "core2/scene/Node.hpp"

namespace declara
{
  namespace app
  {
    // 空ノード用TypeTag
    class EmptyTypeTag
    {
    };

    // 空ノード用Props（NodePropsBaseを継承、TypeTag付き）
    struct EmptyProps : public core::scene::NodePropsBase<EmptyProps>
    {
      typedef EmptyTypeTag TypeTag;
      bool operator<(const core::scene::PropsBase &rhs) const { return false; }
    };

    // 空ノード本体
    class EmptyNode : public core::scene::Node
    {
    public:
      typedef EmptyTypeTag TypeTag;
      const EmptyProps &props;
      EmptyNode(const EmptyProps &p) : props(p) {}
      virtual void compose() {}
    };

    // 空ノードのDefinition
    struct EmptyDefinition : public core::scene::NodeDefinition<EmptyProps, EmptyNode>
    {
      EmptyDefinition() : NodeDefinition() {}
      EmptyDefinition(EmptyProps &p) : NodeDefinition(p) {}
      using core::scene::NodeDefinition<EmptyProps, EmptyNode>::create;
    };
    // DSL向け短縮名
    typedef EmptyDefinition Empty;
  } // namespace app
} // namespace declara

#endif // DECLARA_APP2_EMPTY_HPP
