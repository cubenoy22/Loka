#ifndef LOKA_APP2_BUTTON_HPP
#define LOKA_APP2_BUTTON_HPP

#include "core/State.hpp"
#include "loka/core/String.hpp"
#include "core2/scene/Node.hpp"

namespace declara
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
      virtual State<loka::core::String> *getText() const = 0;
      virtual State<bool> *getEnabled() const = 0;
      virtual EmitterState *getOnClick() const = 0;
    };

    struct ButtonProps : public declara::core::scene::NodePropsBase<ButtonProps>, public IButtonProps
    {
      typedef ButtonTypeTag TypeTag;
      typedef ButtonNode NodeType;
      State<loka::core::String> *text_;
      State<bool> *enabled_;
      EmitterState *onClick_;
      ButtonProps() : text_(0), enabled_(0), onClick_(0) {}
      ButtonProps &text(State<loka::core::String> *t)
      {
        this->text_ = t;
        return *this;
      }
      ButtonProps &text(const loka::core::String &s)
      {
        this->text_ = declara::core::StaticState<loka::core::String>(s);
        return *this;
      }
      ButtonProps &text(const char *s)
      {
        this->text_ = declara::core::StaticState<loka::core::String>(loka::core::String::Literal(s));
        return *this;
      }
      ButtonProps &enabled(State<bool> *e)
      {
        this->enabled_ = e;
        return *this;
      }
      ButtonProps &onClick(EmitterState *e)
      {
        this->onClick_ = e;
        return *this;
      }
      // --- IButtonProps 実装 ---
      virtual State<loka::core::String> *getText() const { return text_; }
      virtual State<bool> *getEnabled() const { return enabled_; }
      virtual EmitterState *getOnClick() const { return onClick_; }
      int hash() const
      {
        int h = 17;
        h = h * 31 + (int)(long)text_;
        h = h * 31 + (int)(long)enabled_;
        h = h * 31 + (int)(long)onClick_;
        return h;
      }
      bool operator<(const declara::core::scene::PropsBase &rhs) const
      {
        const ButtonProps *b = dynamic_cast<const ButtonProps *>(&rhs);
        if (!b)
          return false;
        if (text_ != b->text_)
          return text_ < b->text_;
        return enabled_ < b->enabled_;
      }
    };

    class ButtonNode : public declara::core::scene::Node
    {
    public:
      typedef ButtonTypeTag TypeTag;
      ButtonProps props;
      ButtonNode(const ButtonProps &p) : props(p) {}
    };

    struct ButtonDefinition : public declara::core::scene::NodeDefinition<ButtonProps, ButtonNode>
    {
      ButtonDefinition() : NodeDefinition() {}
      ButtonDefinition(const ButtonProps &p) : NodeDefinition(p) {}
      ButtonDefinition(const char *text) : NodeDefinition()
      {
        this->props.text_ = declara::core::StaticState<loka::core::String>(loka::core::String::Literal(text));
      }
      ButtonDefinition(State<loka::core::String> *text) : NodeDefinition()
      {
        this->props.text_ = text;
      }
      ButtonDefinition(const char *text, EmitterState *onClick) : NodeDefinition()
      {
        this->props.text_ = declara::core::StaticState<loka::core::String>(loka::core::String::Literal(text));
        this->props.onClick_ = onClick;
      }
      ButtonDefinition(State<loka::core::String> *text, EmitterState *onClick) : NodeDefinition()
      {
        this->props.text_ = text;
        this->props.onClick_ = onClick;
      }

      ButtonDefinition &onClick(EmitterState *e)
      {
        this->props.onClick_ = e;
        return *this;
      }

      ButtonDefinition &enabled(State<bool> *b)
      {
        this->props.enabled_ = b;
        return *this;
      }

      using declara::core::scene::NodeDefinition<ButtonProps, ButtonNode>::create;
    };
    // DSL向け短縮名
    typedef ButtonDefinition Button;

    inline ButtonNode *createNode(ButtonProps &props)
    {
      return new ButtonNode(props);
    }
  } // namespace app
} // namespace declara

#endif // LOKA_APP2_BUTTON_HPP
