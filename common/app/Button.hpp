#ifndef DECLARA_BUTTON_HPP
#define DECLARA_BUTTON_HPP

#include <string>
#include "core/State.hpp"
#include "core/PlatformContext.hpp"
#include "core/SceneComponent.hpp"
#include "core/SceneNode.hpp"
#include "core/SceneNodeFactory.hpp"

static State<bool> BUTTON_DEFAULT_ENABLED(true);

struct ButtonTypeTag
{
};

struct ButtonProps : public NodePropsBase<ButtonProps>
{
  typedef ButtonTypeTag TypeTag;

  State<std::string> *text;
  State<bool> *enabled;
  ButtonProps() : text(0), enabled(&BUTTON_DEFAULT_ENABLED) {}
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
  bool operator<(const PropsBase &rhs) const
  {
    const ButtonProps *b = dynamic_cast<const ButtonProps *>(&rhs);
    if (!b)
      return false;
    if (text != b->text)
      return text < b->text;
    return enabled < b->enabled;
  }
  static SceneNode *createNode(const ButtonProps &props);
};

class SceneNodeButton : public SceneNode
{
public:
  typedef ButtonTypeTag TypeTag;

  ButtonProps props;
  SceneNodeButton(const ButtonProps &p) : props(p) {}

  // ボタンのクリックイベント（購読側がbind/bindDeferで副作用を登録）
  EmitterState clickEvent;

  State<std::string> *text() const { return props.text; }
  State<bool> *enabled() const { return props.enabled; }
};

// --- ButtonDefinition: ButtonProps/SceneNodeButton専用の具象版 ---
typedef NodeDefinition<ButtonProps, SceneNodeButton> ButtonDefinition;

inline SceneNode *ButtonProps::createNode(const ButtonProps &props) { return new SceneNodeButton(props); }

#endif // DECLARA_BUTTON_HPP
