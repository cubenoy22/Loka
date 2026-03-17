#ifndef LOKA_APP2_EDITTEXT_HPP
#define LOKA_APP2_EDITTEXT_HPP

#include <string>
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
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
      short toolboxControlId_;
      int controlTag_;
      EditTextProps() : text_(0), toolboxControlId_(0), controlTag_(0) {}
      EditTextProps(loka::core::State<loka::core::String> *state) : text_(state), toolboxControlId_(0), controlTag_(0) {}
      EditTextProps &text(loka::core::State<loka::core::String> *state)
      {
        this->text_ = state;
        return *this;
      }
      EditTextProps &toolboxControl(short id)
      {
        this->toolboxControlId_ = id;
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
        if (toolboxControlId_ != other.toolboxControlId_)
          return toolboxControlId_ < other.toolboxControlId_;
        return text_ < other.text_;
      }
    };

    class EditTextNode : public scene::Node
    {
    public:
      typedef EditTextTypeTag TypeTag;
      EditTextProps props;
      EditTextNode(const EditTextProps &p) : props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_EDIT_TEXT; }
      virtual EditTextNode *asEditTextNode() { return this; }
      virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
      {
        if (this->props.text_)
        {
          registrar.observe(this->props.text_, loka::app::scene::NODE_DIRTY_PROPS);
        }
      }
    };

    struct EditTextDefinition : public scene::NodeDefinition<EditTextProps, EditTextNode>, public scene::TestIdDslMixin<EditTextDefinition>
    {
      EditTextDefinition() : loka::app::scene::NodeDefinition<EditTextProps, EditTextNode>() {}
      EditTextDefinition(const EditTextProps &p) : loka::app::scene::NodeDefinition<EditTextProps, EditTextNode>(p) {}
      EditTextDefinition(loka::core::State<loka::core::String> *state) : loka::app::scene::NodeDefinition<EditTextProps, EditTextNode>(EditTextProps(state)) {}

      EditTextDefinition &toolboxControl(short id)
      {
        this->props.toolboxControlId_ = id;
        return *this;
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
