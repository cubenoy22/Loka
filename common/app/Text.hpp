#ifndef DECLARA_TEXT_HPP
#define DECLARA_TEXT_HPP

#include <string>
#include "core/State.hpp"
#include "core/PlatformContext.hpp"
#include "core/SceneNode.hpp"
#include "core/SceneNodeFactory.hpp"

struct TextTypeTag
{
};

struct TextProps : public NodePropsBase<TextProps>
{
  typedef TextTypeTag TypeTag;

  State<std::string> *text;
  TextProps() : text(0) {}
  TextProps &setText(State<std::string> *t)
  {
    text = t;
    return *this;
  }
  TextProps &setText(const std::string &s)
  {
    text = StaticState<std::string>(s);
    return *this;
  }
  TextProps &setText(const char *s)
  {
    text = StaticState<std::string>(std::string(s));
    return *this;
  }
  bool operator<(const PropsBase &rhs) const
  {
    const TextProps *b = dynamic_cast<const TextProps *>(&rhs);
    if (!b)
      return false;
    return text < b->text;
  }
  static SceneNode *createNode(const TextProps &props);
};

class SceneNodeText : public SceneNode
{
public:
  typedef TextTypeTag TypeTag;

  TextProps props;
  SceneNodeText(const TextProps &p) : props(p) {}

  State<std::string> *text() const { return props.text; }
};

// --- TextDefinition: TextProps/SceneNodeText専用の具象版 ---
typedef NodeDefinition<TextProps, SceneNodeText> TextDefinition;

inline SceneNode *TextProps::createNode(const TextProps &props) { return new SceneNodeText(props); }

#endif // DECLARA_TEXT_HPP
