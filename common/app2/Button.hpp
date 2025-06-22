#ifndef DECLARA_APP2_BUTTON_HPP
#define DECLARA_APP2_BUTTON_HPP

#include <string>
#include "core2/scene/Node.hpp"
#include "core/State.hpp"

namespace declara
{
  namespace app
  {
    class ButtonTypeTag
    {
    };

    struct ButtonProps : public core::scene::NodePropsBase<ButtonProps>
    {
      typedef ButtonTypeTag TypeTag;
      State<std::string> *text;
      State<bool> *enabled;
      ButtonProps() : text(0), enabled(0) {}
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
      int hash() const
      {
        int h = 17;
        h = h * 31 + (int)(long)text;
        h = h * 31 + (int)(long)enabled;
        return h;
      }
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        const ButtonProps *b = dynamic_cast<const ButtonProps *>(&rhs);
        if (!b)
          return false;
        if (text != b->text)
          return text < b->text;
        return enabled < b->enabled;
      }
    };

    class ButtonNode : public core::scene::Node
    {
    public:
      typedef ButtonTypeTag TypeTag;
      ButtonProps props;
      ButtonNode(const ButtonProps &p) : props(p) {}
    };

    struct ButtonDefinition : public core::scene::NodeDefinition<ButtonProps, ButtonNode>
    {
      ButtonDefinition() : NodeDefinition() {}
      ButtonDefinition(const ButtonProps &p) : NodeDefinition(p) {}
      using core::scene::NodeDefinition<ButtonProps, ButtonNode>::create;
    };
    // DSL向け短縮名
    typedef ButtonDefinition Button;

    inline ButtonNode *createNode(const ButtonProps &props)
    {
      return new ButtonNode(props);
    }
  } // namespace app
} // namespace declara

#endif // DECLARA_APP2_BUTTON_HPP
