#ifndef LOKA_APP2_TEXT_HPP
#define LOKA_APP2_TEXT_HPP

#include "loka/core/State.hpp"
#include "app/scene/Node.hpp"
#include "loka/core/String.hpp"
#include "app/Attr.hpp"
#include <cassert>

namespace loka
{
  namespace app
  {
    struct TextTypeTag
    {
    };

    class TextNode;

    struct TextProps : public scene::NodePropsBase<TextProps>
    {
      typedef TextTypeTag TypeTag;
      typedef TextNode NodeType;
      loka::core::State<loka::core::String> *text_;
      loka::core::MutableState<loka::core::String> ownedText;
      bool ownsText;
      TextAttr attr_;
      bool hasAttr_;
      TextProps() : text_(0), ownedText(), ownsText(false), attr_(), hasAttr_(false) {}
      TextProps(loka::core::State<loka::core::String> *state) : text_(state), ownedText(), ownsText(false), attr_(), hasAttr_(false) {}
      TextProps(const loka::core::String &value) : text_(0), ownedText(value), ownsText(true), attr_(), hasAttr_(false)
      {
        text_ = &ownedText;
      }
      TextProps(const char *value) : text_(0), ownedText(loka::core::String::Literal(value)), ownsText(true), attr_(), hasAttr_(false)
      {
        text_ = &ownedText;
      }
      TextProps(const TextProps &other)
          : text_(other.text_), ownedText(other.ownedText), ownsText(other.ownsText), attr_(other.attr_), hasAttr_(other.hasAttr_)
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
          attr_ = other.attr_;
          hasAttr_ = other.hasAttr_;
          if (ownsText)
          {
            text_ = &ownedText;
          }
        }
        return *this;
      }
      TextProps &text(loka::core::State<loka::core::String> *state)
      {
        this->text_ = state;
        ownsText = false;
        return *this;
      }
      TextProps &text(const loka::core::String &value)
      {
        ownedText = loka::core::MutableState<loka::core::String>(value);
        this->text_ = &ownedText;
        ownsText = true;
        return *this;
      }
      TextProps &text(const char *value)
      {
        return text(loka::core::String::Literal(value));
      }

      TextProps &attr(const TextAttr &value)
      {
        assert(!this->hasAttr_ && "Text.attr() can only be set once per node");
        this->attr_ = value;
        this->hasAttr_ = true;
        return *this;
      }

      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const TextProps &other = static_cast<const TextProps &>(rhs);
        if (text_ != other.text_)
          return text_ < other.text_;
        if (hasAttr_ != other.hasAttr_)
          return hasAttr_ < other.hasAttr_;
        return attr_ < other.attr_;
      }
    };

    class TextNode : public scene::Node
    {
    public:
      typedef TextTypeTag TypeTag;
      TextProps props;
      TextNode(const TextProps &p) : props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_TEXT; }
      virtual TextNode *asTextNode() { return this; }
    };

    struct TextDefinition : public scene::NodeDefinition<TextProps, TextNode>
    {
      TextDefinition() : loka::app::scene::NodeDefinition<TextProps, TextNode>() {}
      TextDefinition(const TextProps &p) : loka::app::scene::NodeDefinition<TextProps, TextNode>(p) {}
      TextDefinition(const char *value) : loka::app::scene::NodeDefinition<TextProps, TextNode>(TextProps(value)) {}
      TextDefinition(const loka::core::String &value) : loka::app::scene::NodeDefinition<TextProps, TextNode>(TextProps(value)) {}
      TextDefinition(loka::core::State<loka::core::String> *state) : loka::app::scene::NodeDefinition<TextProps, TextNode>(TextProps(state)) {}

      TextDefinition &attr(const TextAttr &value)
      {
        this->props.attr(value);
        return *this;
      }
    };

    typedef TextDefinition Text;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_TEXT_HPP
