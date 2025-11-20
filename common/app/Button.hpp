#ifndef DECLARA_APP2_BUTTON_HPP
#define DECLARA_APP2_BUTTON_HPP

#include <string>
#include "core/State.hpp"
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
      virtual State<std::string> *getText() const = 0;
      virtual State<bool> *getEnabled() const = 0;
      virtual EmitterState *getOnClick() const = 0;
    };

    struct ButtonProps : public declara::core::scene::NodePropsBase<ButtonProps>, public IButtonProps
    {
      typedef ButtonTypeTag TypeTag;
      typedef ButtonNode NodeType;
      State<std::string> *text;
      State<bool> *enabled;
      EmitterState *onClick;
      ButtonProps() : text(0), enabled(0), onClick(0) {}
      ButtonProps &setText(State<std::string> *t)
      {
        text = t;
        return *this;
      }
      ButtonProps &setText(const std::string &s)
      {
        text = StaticState<std::string>(s);
        return *this;
      }
      ButtonProps &setText(const char *s)
      {
        text = StaticState<std::string>(std::string(s));
        return *this;
      }
      ButtonProps &setEnabled(State<bool> *e)
      {
        enabled = e;
        return *this;
      }
      ButtonProps &setOnClick(EmitterState *e)
      {
        onClick = e;
        return *this;
      }
      // --- IButtonProps 実装 ---
      virtual State<std::string> *getText() const { return text; }
      virtual State<bool> *getEnabled() const { return enabled; }
      virtual EmitterState *getOnClick() const { return onClick; }
      int hash() const
      {
        int h = 17;
        h = h * 31 + (int)(long)text;
        h = h * 31 + (int)(long)enabled;
        h = h * 31 + (int)(long)onClick;
        return h;
      }
      bool operator<(const declara::core::scene::PropsBase &rhs) const
      {
        const ButtonProps *b = dynamic_cast<const ButtonProps *>(&rhs);
        if (!b)
          return false;
        if (text != b->text)
          return text < b->text;
        return enabled < b->enabled;
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

#endif // DECLARA_APP2_BUTTON_HPP
