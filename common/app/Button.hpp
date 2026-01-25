#ifndef LOKA_APP2_BUTTON_HPP
#define LOKA_APP2_BUTTON_HPP

#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    class ButtonTypeTag
    {
    };

    // Forward declaration
    class ButtonNode;

    class IButtonProps
    {
    public:
      typedef ButtonTypeTag TypeTag;
      virtual ~IButtonProps() {}
      virtual loka::core::State<loka::core::String> *getText() const = 0;
      virtual loka::core::State<bool> *getEnabled() const = 0;
      virtual loka::core::EmitterState *getOnClick() const = 0;
    };

    struct ButtonProps : public loka::app::scene::NodePropsBase<ButtonProps>, public IButtonProps
    {
      typedef ButtonTypeTag TypeTag;
      typedef ButtonNode NodeType;
      loka::core::State<loka::core::String> *text_;
      loka::core::State<bool> *enabled_;
      loka::core::EmitterState *onClick_;
      short toolboxControlId_;
      int controlTag_;
      ButtonProps() : text_(0), enabled_(0), onClick_(0), toolboxControlId_(0), controlTag_(0) {}
      ButtonProps &text(loka::core::State<loka::core::String> *t)
      {
        this->text_ = t;
        return *this;
      }
      ButtonProps &text(const loka::core::String &s)
      {
        this->text_ = loka::core::StaticState<loka::core::String>(s);
        return *this;
      }
      ButtonProps &text(const char *s)
      {
        this->text_ = loka::core::StaticState<loka::core::String>(loka::core::String::Literal(s));
        return *this;
      }
      ButtonProps &enabled(loka::core::State<bool> *e)
      {
        this->enabled_ = e;
        return *this;
      }
      ButtonProps &onClick(loka::core::EmitterState *e)
      {
        this->onClick_ = e;
        return *this;
      }
      ButtonProps &toolboxControl(short id)
      {
        this->toolboxControlId_ = id;
        return *this;
      }
      ButtonProps &controlTag(int tag)
      {
        this->controlTag_ = tag;
        return *this;
      }
      // --- IButtonProps 実装 ---
      virtual loka::core::State<loka::core::String> *getText() const { return text_; }
      virtual loka::core::State<bool> *getEnabled() const { return enabled_; }
      virtual loka::core::EmitterState *getOnClick() const { return onClick_; }
      int hash() const
      {
        std::size_t h = 17;
        h = h * 31 + reinterpret_cast<std::size_t>(text_);
        h = h * 31 + reinterpret_cast<std::size_t>(enabled_);
        h = h * 31 + reinterpret_cast<std::size_t>(onClick_);
        h = h * 31 + static_cast<std::size_t>(toolboxControlId_);
        h = h * 31 + static_cast<std::size_t>(controlTag_);
        return static_cast<int>(h);
      }
      bool operator<(const loka::app::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const ButtonProps &b = static_cast<const ButtonProps &>(rhs);
        if (text_ != b.text_)
          return text_ < b.text_;
        if (controlTag_ != b.controlTag_)
          return controlTag_ < b.controlTag_;
        if (toolboxControlId_ != b.toolboxControlId_)
          return toolboxControlId_ < b.toolboxControlId_;
        return enabled_ < b.enabled_;
      }
    };

    class ButtonNode : public loka::app::scene::Node
    {
    public:
      typedef ButtonTypeTag TypeTag;
      ButtonProps props;
      ButtonNode(const ButtonProps &p) : props(p) {}
      virtual loka::app::scene::NodeKind kind() const { return loka::app::scene::NODE_KIND_BUTTON; }
      virtual ButtonNode *asButtonNode() { return this; }
    };

    struct ButtonDefinition : public loka::app::scene::NodeDefinition<ButtonProps, ButtonNode>
    {
      ButtonDefinition() : NodeDefinition() {}
      ButtonDefinition(const ButtonProps &p) : NodeDefinition(p) {}
      ButtonDefinition(const char *text) : NodeDefinition()
      {
        this->props.text_ = loka::core::StaticState<loka::core::String>(loka::core::String::Literal(text));
      }
      ButtonDefinition(loka::core::State<loka::core::String> *text) : NodeDefinition()
      {
        this->props.text_ = text;
      }
      ButtonDefinition(const char *text, loka::core::EmitterState *onClick) : NodeDefinition()
      {
        this->props.text_ = loka::core::StaticState<loka::core::String>(loka::core::String::Literal(text));
        this->props.onClick_ = onClick;
      }
      ButtonDefinition(loka::core::State<loka::core::String> *text, loka::core::EmitterState *onClick) : NodeDefinition()
      {
        this->props.text_ = text;
        this->props.onClick_ = onClick;
      }

      ButtonDefinition &onClick(loka::core::EmitterState *e)
      {
        this->props.onClick_ = e;
        return *this;
      }

      ButtonDefinition &enabled(loka::core::State<bool> *b)
      {
        this->props.enabled_ = b;
        return *this;
      }

      ButtonDefinition &toolboxControl(short id)
      {
        this->props.toolboxControlId_ = id;
        return *this;
      }
      ButtonDefinition &controlTag(int tag)
      {
        this->props.controlTag_ = tag;
        return *this;
      }

      using loka::app::scene::NodeDefinition<ButtonProps, ButtonNode>::create;
    };
    // DSL向け短縮名
    typedef ButtonDefinition Button;

    inline ButtonNode *createNode(ButtonProps &props)
    {
      return new ButtonNode(props);
    }
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_BUTTON_HPP
