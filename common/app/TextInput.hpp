#ifndef DECLARA_TEXTINPUT_HPP
#define DECLARA_TEXTINPUT_HPP

#include <string>
#include "core/PlatformContext.hpp"
#include "core/SceneNode.hpp"
#include "core/State.hpp"
#include "core/SceneNodeFactory.hpp"

struct TextInputTypeTag
{
};

struct TextInputProps : public NodePropsBase<TextInputProps>
{
  typedef TextInputTypeTag TypeTag;

  State<std::string> *text;
  State<bool> *enabled;
  EmitterState *onChange;
  TextInputProps() : text(0), enabled(0), onChange(0) {}
  TextInputProps &setText(State<std::string> *t)
  {
    text = t;
    return *this;
  }
  TextInputProps &setText(const std::string &s)
  {
    text = StaticState<std::string>(s);
    return *this;
  }
  TextInputProps &setText(const char *s)
  {
    text = StaticState<std::string>(std::string(s));
    return *this;
  }
  TextInputProps &setEnabled(State<bool> *e)
  {
    enabled = e;
    return *this;
  }
  TextInputProps &setOnChange(EmitterState *e)
  {
    onChange = e;
    return *this;
  }
  static SceneNode *createNode(const TextInputProps &props);
  bool operator<(const PropsBase &rhs) const
  {
    const TextInputProps *b = dynamic_cast<const TextInputProps *>(&rhs);
    if (!b)
      return false;
    if (text != b->text)
      return text < b->text;
    if (enabled != b->enabled)
      return enabled < b->enabled;
    return onChange < b->onChange;
  }
};

class SceneNodeTextInput : public SceneNode
{
public:
  typedef TextInputTypeTag TypeTag;

  TextInputProps props;
  SceneNodeTextInput(const TextInputProps &p) : props(p) {}

  State<std::string> *text() const { return props.text; }
};

// --- TextInputDefinition: TextInputProps/SceneNodeTextInput専用の具象版 ---
typedef NodeDefinition<TextInputProps, SceneNodeTextInput> TextInputDefinition;

inline SceneNode *TextInputProps::createNode(const TextInputProps &props) { return new SceneNodeTextInput(props); }

#endif // DECLARA_TEXTINPUT_HPP
