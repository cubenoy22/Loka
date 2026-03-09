#ifndef LOKA_APP2_EMPTY_HPP
#define LOKA_APP2_EMPTY_HPP

// Empty node definition following Carbon/Toolbox style
// Follows same interface/naming conventions as Button/Box

#include "app/scene/Node.hpp"

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
    struct EmptyProps : public scene::NodePropsBase<EmptyProps>
    {
      typedef EmptyTypeTag TypeTag;
      typedef EmptyNode NodeType;
      EmptyProps() {}
      bool operator<(const scene::PropsBase &rhs) const { return false; }
    };

    // Empty node body
    class EmptyNode : public scene::Node
    {
    public:
      typedef EmptyTypeTag TypeTag;
      const EmptyProps &props;
      EmptyNode(const EmptyProps &p) : props(p) {}
      virtual void compose() {}
    };

    // Definition for Empty node
    struct EmptyDefinition : public scene::NodeDefinition<EmptyProps, EmptyNode>, public scene::TestIdDslMixin<EmptyDefinition>
    {
      EmptyDefinition() : scene::NodeDefinition<EmptyProps, EmptyNode>() {}
      EmptyDefinition(const EmptyProps &p) : scene::NodeDefinition<EmptyProps, EmptyNode>(p) {}
      using scene::NodeDefinition<EmptyProps, EmptyNode>::create;
    };
    // Short name for DSL
    typedef EmptyDefinition Empty;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_EMPTY_HPP
