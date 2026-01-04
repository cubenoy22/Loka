#ifndef DECLARA_APP2_TEXT_HPP
#define DECLARA_APP2_TEXT_HPP

#include <string>
#include "core/State.hpp"
#include "core2/scene/Node.hpp"

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
      State<std::string> *text_;
      MutableState<std::string> ownedText;
      bool ownsText;
      TextProps() : text_(0), ownedText(), ownsText(false) {}
      TextProps(const std::string &value) : text_(0), ownedText(value), ownsText(true)
      {
        text_ = &ownedText;
      }
      TextProps(State<std::string> *state) : text_(state), ownedText(), ownsText(false) {}
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
      TextProps &text(State<std::string> *state)
      {
        this->text_ = state;
        ownsText = false;
        return *this;
      }
      TextProps &text(const std::string &value)
      {
        ownedText = MutableState<std::string>(value);
        this->text_ = &ownedText;
        ownsText = true;
        return *this;
      }
      TextProps &text(const char *value)
      {
        return text(std::string(value));
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
    };

    struct TextDefinition : public core::scene::NodeDefinition<TextProps, TextNode>
    {
      TextDefinition() : NodeDefinition() {}
      TextDefinition(const TextProps &p) : NodeDefinition(p) {}
      TextDefinition(const std::string &value) : NodeDefinition(TextProps(value)) {}
      TextDefinition(const char *value) : NodeDefinition(TextProps(value)) {}
      TextDefinition(State<std::string> *state) : NodeDefinition(TextProps(state)) {}
    };

    typedef TextDefinition Text;
  } // namespace app
} // namespace declara

#endif // DECLARA_APP2_TEXT_HPP
