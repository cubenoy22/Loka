#ifndef LOKA_APP2_EDITTEXT_HPP
#define LOKA_APP2_EDITTEXT_HPP

#include <string>
#include "core/State.hpp"
#include "core2/scene/Node.hpp"

namespace declara
{
  namespace app
  {
    struct EditTextTypeTag
    {
    };

    class EditTextNode;

    struct EditTextProps : public core::scene::NodePropsBase<EditTextProps>
    {
      typedef EditTextTypeTag TypeTag;
      typedef EditTextNode NodeType;
      State<std::string> *text_;
      EditTextProps() : text_(0) {}
      EditTextProps(State<std::string> *state) : text_(state) {}
      EditTextProps &text(State<std::string> *state)
      {
        this->text_ = state;
        return *this;
      }
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        const EditTextProps *other = dynamic_cast<const EditTextProps *>(&rhs);
        if (!other)
        {
          return false;
        }
        return text_ < other->text_;
      }
    };

    class EditTextNode : public core::scene::Node
    {
    public:
      typedef EditTextTypeTag TypeTag;
      EditTextProps props;
      EditTextNode(const EditTextProps &p) : props(p) {}
    };

    struct EditTextDefinition : public core::scene::NodeDefinition<EditTextProps, EditTextNode>
    {
      EditTextDefinition() : NodeDefinition() {}
      EditTextDefinition(const EditTextProps &p) : NodeDefinition(p) {}
      EditTextDefinition(State<std::string> *state) : NodeDefinition(EditTextProps(state)) {}
    };

    typedef EditTextDefinition EditText;
  } // namespace app
} // namespace declara

#endif // LOKA_APP2_EDITTEXT_HPP
