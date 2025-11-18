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
      State<std::string> *text;
      MutableState<std::string> ownedText;
      bool ownsText;
      TextProps() : text(0), ownedText(), ownsText(false) {}
      TextProps(const TextProps &other)
          : text(other.text), ownedText(other.ownedText), ownsText(other.ownsText)
      {
        if (ownsText)
        {
          text = &ownedText;
        }
      }
      TextProps &operator=(const TextProps &other)
      {
        if (this != &other)
        {
          text = other.text;
          ownedText = other.ownedText;
          ownsText = other.ownsText;
          if (ownsText)
          {
            text = &ownedText;
          }
        }
        return *this;
      }
      TextProps &setText(State<std::string> *state)
      {
        text = state;
        ownsText = false;
        return *this;
      }
      TextProps &setText(const std::string &value)
      {
        ownedText = MutableState<std::string>(value);
        text = &ownedText;
        ownsText = true;
        return *this;
      }
      TextProps &setText(const char *value)
      {
        return setText(std::string(value));
      }
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        const TextProps *other = dynamic_cast<const TextProps *>(&rhs);
        if (!other)
        {
          return false;
        }
        return text < other->text;
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
    };

    typedef TextDefinition Text;
  } // namespace app
} // namespace declara

#endif // DECLARA_APP2_TEXT_HPP
