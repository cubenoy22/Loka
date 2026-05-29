#ifndef LOKA_APP2_TEXT_HPP
#define LOKA_APP2_TEXT_HPP

#include <cassert>
#include "core/State.hpp"
#include "app/scene/Node.hpp"
#include "core/String.hpp"

namespace loka
{
  namespace app
  {
    enum TextWeight
    {
      TEXT_WEIGHT_NORMAL = 0,
      TEXT_WEIGHT_BOLD = 1
    };

    enum TextWrap
    {
      TEXT_WRAP_NONE = 0,
      TEXT_WRAP_WORD = 1,
      TEXT_WRAP_CHAR = 2
    };

    enum TextTruncation
    {
      TEXT_TRUNCATION_NONE = 0,
      TEXT_TRUNCATION_CLIP = 1,
      TEXT_TRUNCATION_ELLIPSIS = 2
    };

    struct TextAttr
    {
      TextAttr()
          : hasFontSizeValue_(false),
            fontSizeValue_(0),
            fontSizeState_(0),
            hasWeightValue_(false),
            weightValue_(TEXT_WEIGHT_NORMAL),
            hasWrapValue_(false),
            wrapValue_(TEXT_WRAP_NONE),
            hasTruncationValue_(false),
            truncationValue_(TEXT_TRUNCATION_NONE)
      {
      }

      TextAttr &fontSize(int value)
      {
        this->hasFontSizeValue_ = true;
        this->fontSizeValue_ = value;
        this->fontSizeState_ = 0;
        return *this;
      }

      TextAttr &fontSize(loka::core::State<int> *state)
      {
        this->hasFontSizeValue_ = false;
        this->fontSizeState_ = state;
        return *this;
      }

      TextAttr &weight(TextWeight value)
      {
        this->hasWeightValue_ = true;
        this->weightValue_ = value;
        return *this;
      }

      TextAttr &wrap(TextWrap value)
      {
        this->hasWrapValue_ = true;
        this->wrapValue_ = value;
        return *this;
      }

      TextAttr &truncation(TextTruncation value)
      {
        this->hasTruncationValue_ = true;
        this->truncationValue_ = value;
        return *this;
      }

      bool operator==(const TextAttr &other) const
      {
        return this->hasFontSizeValue_ == other.hasFontSizeValue_ && this->fontSizeValue_ == other.fontSizeValue_
               && this->fontSizeState_ == other.fontSizeState_ && this->hasWeightValue_ == other.hasWeightValue_
               && this->weightValue_ == other.weightValue_ && this->hasWrapValue_ == other.hasWrapValue_
               && this->wrapValue_ == other.wrapValue_ && this->hasTruncationValue_ == other.hasTruncationValue_
               && this->truncationValue_ == other.truncationValue_;
      }

      bool operator<(const TextAttr &other) const
      {
        if (this->hasFontSizeValue_ != other.hasFontSizeValue_)
          return this->hasFontSizeValue_ < other.hasFontSizeValue_;
        if (this->fontSizeValue_ != other.fontSizeValue_)
          return this->fontSizeValue_ < other.fontSizeValue_;
        if (this->fontSizeState_ != other.fontSizeState_)
          return this->fontSizeState_ < other.fontSizeState_;
        if (this->hasWeightValue_ != other.hasWeightValue_)
          return this->hasWeightValue_ < other.hasWeightValue_;
        if (this->weightValue_ != other.weightValue_)
          return this->weightValue_ < other.weightValue_;
        if (this->hasWrapValue_ != other.hasWrapValue_)
          return this->hasWrapValue_ < other.hasWrapValue_;
        if (this->wrapValue_ != other.wrapValue_)
          return this->wrapValue_ < other.wrapValue_;
        if (this->hasTruncationValue_ != other.hasTruncationValue_)
          return this->hasTruncationValue_ < other.hasTruncationValue_;
        return this->truncationValue_ < other.truncationValue_;
      }

      bool hasFontSizeValue_;
      int fontSizeValue_;
      loka::core::State<int> *fontSizeState_;
      bool hasWeightValue_;
      TextWeight weightValue_;
      bool hasWrapValue_;
      TextWrap wrapValue_;
      bool hasTruncationValue_;
      TextTruncation truncationValue_;
    };

    struct TextTypeTag
    {
    };

    class TextNode;
    struct TextDefinitionWithAttr;

    struct TextProps : public scene::NodePropsBase<TextProps>
    {
      typedef TextTypeTag TypeTag;
      typedef TextNode NodeType;
      loka::core::State<loka::core::String> *text_;
      loka::core::MutableState<loka::core::String> ownedText;
      bool ownsText;
      TextAttr attr_;
      bool hasAttr_;
      TextProps()
          : text_(0),
            ownedText(),
            ownsText(false),
            attr_(),
            hasAttr_(false)
      {
      }
      TextProps(loka::core::State<loka::core::String> *state)
          : text_(state),
            ownedText(),
            ownsText(false),
            attr_(),
            hasAttr_(false)
      {
      }
      TextProps(const loka::core::String &value)
          : text_(0),
            ownedText(value),
            ownsText(true),
            attr_(),
            hasAttr_(false)
      {
        text_ = &ownedText;
      }
      TextProps(const char *value)
          : text_(0),
            ownedText(loka::core::String::Literal(value)),
            ownsText(true),
            attr_(),
            hasAttr_(false)
      {
        text_ = &ownedText;
      }
      TextProps(const TextProps &other)
          : text_(other.text_),
            ownedText(other.ownedText),
            ownsText(other.ownsText),
            attr_(other.attr_),
            hasAttr_(other.hasAttr_)
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

      static int compareTextState(loka::core::State<loka::core::String> *left,
                                  loka::core::State<loka::core::String> *right)
      {
        if (left == right)
        {
          return 0;
        }
        if (!left)
        {
          return -1;
        }
        if (!right)
        {
          return 1;
        }
        return left->get().compare(right->get());
      }

      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const TextProps &other = static_cast<const TextProps &>(rhs);
        int textCompare = compareTextState(text_, other.text_);
        if (textCompare != 0)
          return textCompare < 0;
        if (hasAttr_ != other.hasAttr_)
          return hasAttr_ < other.hasAttr_;
        return attr_ < other.attr_;
      }
    };

    class TextNode : public scene::Node, public scene::IProjectedLayoutNode
    {
    public:
      typedef TextTypeTag TypeTag;
      TextProps props;
      TextNode(const TextProps &p)
          : props(p)
      {
      }
      virtual scene::NodeKind kind() const
      {
        return scene::NODE_KIND_TEXT;
      }
      virtual scene::IProjectedLayoutNode *asProjectedLayoutNode()
      {
        return this;
      }
      virtual const void *nodeTypeKey() const
      {
        return scene::NodeTypeToken<TextNode>();
      }
      virtual TextNode *asTextNode()
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
      virtual void declareDirtySources(scene::DirtySourceRegistrar &registrar)
      {
        if (this->props.text_ && !this->props.ownsText)
        {
          scene::NodeDirtyFlags textFlags = scene::NODE_DIRTY_PROPS;
          if (this->props.hasAttr_ && this->props.attr_.hasWrapValue_ && this->props.attr_.wrapValue_ != TEXT_WRAP_NONE)
          {
            textFlags = static_cast<scene::NodeDirtyFlags>(textFlags | scene::NODE_DIRTY_LAYOUT);
          }
          registrar.markDirtyOnChange(this->props.text_, textFlags);
        }
        if (this->props.hasAttr_ && this->props.attr_.fontSizeState_)
        {
          registrar.markDirtyOnChange(this->props.attr_.fontSizeState_, scene::NODE_DIRTY_LAYOUT);
        }
      }
    };

    struct TextDefinition : public scene::NodeDefinition<TextProps, TextNode>,
                            public scene::TestIdDslMixin<TextDefinition>
    {
      TextDefinition()
          : loka::app::scene::NodeDefinition<TextProps, TextNode>()
      {
      }
      TextDefinition(const TextProps &p)
          : loka::app::scene::NodeDefinition<TextProps, TextNode>(p)
      {
      }
      TextDefinition(const char *value)
          : loka::app::scene::NodeDefinition<TextProps, TextNode>(TextProps(value))
      {
      }
      TextDefinition(const loka::core::String &value)
          : loka::app::scene::NodeDefinition<TextProps, TextNode>(TextProps(value))
      {
      }
      TextDefinition(loka::core::State<loka::core::String> *state)
          : loka::app::scene::NodeDefinition<TextProps, TextNode>(TextProps(state))
      {
      }
      TextDefinitionWithAttr attr(const TextAttr &value) const;
    };

    struct TextDefinitionWithAttr : public scene::NodeDefinition<TextProps, TextNode>,
                                    public scene::TestIdDslMixin<TextDefinitionWithAttr>
    {
      TextDefinitionWithAttr()
          : loka::app::scene::NodeDefinition<TextProps, TextNode>()
      {
      }
      TextDefinitionWithAttr(const TextProps &p)
          : loka::app::scene::NodeDefinition<TextProps, TextNode>(p)
      {
      }
      TextDefinitionWithAttr(const TextDefinition &def)
          : loka::app::scene::NodeDefinition<TextProps, TextNode>(def.props)
      {
        this->copyTestIdPolicyFrom(def);
      }
    };

    inline TextDefinitionWithAttr TextDefinition::attr(const TextAttr &value) const
    {
      TextProps p = this->props;
      p.attr(value);
      TextDefinitionWithAttr result(p);
      result.copyTestIdPolicyFrom(*this);
      return result;
    }

    typedef TextDefinition Text;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_TEXT_HPP
