#ifndef LOKA_APP2_TEXT_HPP
#define LOKA_APP2_TEXT_HPP

#include "core/State.hpp"
#include "core2/scene/Node.hpp"
#include "loka/core/String.hpp"

namespace declara
{
  namespace app
  {
    struct TextTypeTag
    {
    };

    class TextNode;

    struct TextProps : public core::scene::NodePropsBase<TextProps>
    {
      typedef TextTypeTag TypeTag;
      typedef TextNode NodeType;
      State<loka::core::String> *text_;
      MutableState<loka::core::String> ownedText;
      bool ownsText;
      TextProps() : text_(0), ownedText(), ownsText(false) {}
      TextProps(State<loka::core::String> *state) : text_(state), ownedText(), ownsText(false) {}
      TextProps(const loka::core::String &value) : text_(0), ownedText(value), ownsText(true)
      {
        text_ = &ownedText;
      }
      TextProps(const char *value) : text_(0), ownedText(loka::core::String::Literal(value)), ownsText(true)
      {
        text_ = &ownedText;
      }
      TextProps(const TextProps &other)
          : text_(other.text_), ownedText(other.ownedText), ownsText(other.ownsText)
      {
        if (ownsText)
        {
          text_ = &ownedText;
        }
      }
      TextProps &operator=(const TextProps &other)
      {
        if (this != &other)
        {
          text_ = other.text_;
          ownedText = other.ownedText;
          ownsText = other.ownsText;
          if (ownsText)
          {
            text_ = &ownedText;
          }
        }
        return *this;
      }
      TextProps &text(State<loka::core::String> *state)
      {
        this->text_ = state;
        ownsText = false;
        return *this;
      }
      TextProps &text(const loka::core::String &value)
      {
        ownedText = MutableState<loka::core::String>(value);
        this->text_ = &ownedText;
        ownsText = true;
        return *this;
      }
      TextProps &text(const char *value)
      {
        return text(loka::core::String::Literal(value));
      }
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        const TextProps *other = dynamic_cast<const TextProps *>(&rhs);
        if (!other)
        {
          return false;
        }
        return text_ < other->text_;
      }
    };

    class TextNode : public core::scene::Node
    {
    public:
      typedef TextTypeTag TypeTag;
      TextProps props;
      TextNode(const TextProps &p) : props(p) {}
      virtual core::scene::NodeKind kind() const { return core::scene::NODE_KIND_TEXT; }
      virtual TextNode *asTextNode() { return this; }
    };

    struct TextDefinition : public core::scene::NodeDefinition<TextProps, TextNode>
    {
      TextDefinition() : NodeDefinition() {}
      TextDefinition(const TextProps &p) : NodeDefinition(p) {}
      TextDefinition(const char *value) : NodeDefinition(TextProps(value)) {}
      TextDefinition(const loka::core::String &value) : NodeDefinition(TextProps(value)) {}
      TextDefinition(State<loka::core::String> *state) : NodeDefinition(TextProps(state)) {}
    };

    typedef TextDefinition Text;
  } // namespace app
} // namespace declara

#endif // LOKA_APP2_TEXT_HPP
