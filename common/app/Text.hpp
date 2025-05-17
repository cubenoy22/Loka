#ifndef DECLARA_TEXT_HPP
#define DECLARA_TEXT_HPP

#include <string>
#include "core/PlatformContext.hpp"
#include "core/SceneNode.hpp"

class SceneNodeText : public SceneNode
{
public:
  explicit SceneNodeText(State<std::string> *text) : text_(text) {}
  State<std::string> *text() const { return text_; }
  SceneNodeText *setText(State<std::string> *text)
  {
    text_ = text;
    return this;
  }

protected:
  State<std::string> *text_;
};

#endif // DECLARA_TEXT_HPP
