#ifndef LOKA_APP2_EMPTY_HPP
#define LOKA_APP2_EMPTY_HPP

// Empty node definition following Carbon/Toolbox style
// Follows same interface/naming conventions as Button/Box

#include "core2/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    // TypeTag for Empty node
    class EmptyTypeTag
    {
    };

    // Forward declaration
    class EmptyNode;

    // Props for Empty node (inherits NodePropsBase, with TypeTag)
    struct EmptyProps : public core::scene::NodePropsBase<EmptyProps>
    {
      typedef EmptyTypeTag TypeTag;
      typedef EmptyNode NodeType;
      EmptyProps() {}
      bool operator<(const core::scene::PropsBase &rhs) const { return false; }
    };

    // Empty node body
    class EmptyNode : public core::scene::Node
    {
    public:
      typedef EmptyTypeTag TypeTag;
      const EmptyProps &props;
      EmptyNode(const EmptyProps &p) : props(p) {}
      virtual void compose() {}
    };

    // Definition for Empty node
    struct EmptyDefinition : public core::scene::NodeDefinition<EmptyProps, EmptyNode>
    {
      EmptyDefinition() : NodeDefinition() {}
      EmptyDefinition(const EmptyProps &p) : NodeDefinition(p) {}
      using core::scene::NodeDefinition<EmptyProps, EmptyNode>::create;
    };
    // Short name for DSL
    typedef EmptyDefinition Empty;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_EMPTY_HPP
