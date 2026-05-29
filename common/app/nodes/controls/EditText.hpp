#ifndef LOKA_APP2_EDITTEXT_HPP
#define LOKA_APP2_EDITTEXT_HPP

#include <string>
#include "core/State.hpp"
#include "core/String.hpp"
#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct EditTextTypeTag
    {
    };

    class EditTextNode;

    struct EditTextProps : public scene::NodePropsBase<EditTextProps>
    {
      typedef EditTextTypeTag TypeTag;
      typedef EditTextNode NodeType;
      loka::core::State<loka::core::String> *text_;
      int controlTag_;
      EditTextProps()
          : text_(0),
            controlTag_(0)
      {
      }
      EditTextProps(loka::core::State<loka::core::String> *state)
          : text_(state),
            controlTag_(0)
      {
      }
      EditTextProps &text(loka::core::State<loka::core::String> *state)
      {
        this->text_ = state;
        return *this;
      }
      EditTextProps &controlTag(int tag)
      {
        this->controlTag_ = tag;
        return *this;
      }
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const EditTextProps &other = static_cast<const EditTextProps &>(rhs);
        if (controlTag_ != other.controlTag_)
          return controlTag_ < other.controlTag_;
        return text_ < other.text_;
      }
    };

    class EditTextNode : public scene::Node, public scene::IProjectedLayoutNode
    {
    public:
      typedef EditTextTypeTag TypeTag;
      EditTextProps props;
      EditTextNode(const EditTextProps &p)
          : props(p)
      {
      }
      virtual scene::NodeKind kind() const
      {
        return scene::NODE_KIND_EDIT_TEXT;
      }
      virtual scene::IProjectedLayoutNode *asProjectedLayoutNode()
      {
        return this;
      }
      virtual const void *nodeTypeKey() const
      {
        return scene::NodeTypeToken<EditTextNode>();
      }
      virtual EditTextNode *asEditTextNode()
      {
        return this;
      }
      virtual short layoutProjected(scene::IPlatformController *controller, scene::LayoutState &state)
      {
        if (!controller)
        {
          return state.y;
        }
        if (!scene::PrepareProjectedLayout(controller, this, state))
        {
          return state.y;
        }
        return scene::Node::layout(controller, state);
      }
      virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
      {
        if (this->props.text_)
        {
          registrar.markDirtyOnChange(this->props.text_, loka::app::scene::NODE_DIRTY_PROPS);
        }
      }
    };

    struct EditTextDefinition : public scene::NodeDefinition<EditTextProps, EditTextNode>,
                                public scene::TestIdDslMixin<EditTextDefinition>
    {
      EditTextDefinition()
          : loka::app::scene::NodeDefinition<EditTextProps, EditTextNode>()
      {
      }
      EditTextDefinition(const EditTextProps &p)
          : loka::app::scene::NodeDefinition<EditTextProps, EditTextNode>(p)
      {
      }
      EditTextDefinition(loka::core::State<loka::core::String> *state)
          : loka::app::scene::NodeDefinition<EditTextProps, EditTextNode>(EditTextProps(state))
      {
      }

      EditTextDefinition &controlTag(int tag)
      {
        this->props.controlTag_ = tag;
        return *this;
      }
    };

    typedef EditTextDefinition EditText;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_EDITTEXT_HPP
