#ifndef DECLARA_TEXTINPUT_HPP
#define DECLARA_TEXTINPUT_HPP

#include <string>
#include "core/PlatformContext.hpp"
#include "core/SceneNode.hpp"

// --- SceneNodeTextInput: SceneNodeラッパー ---
class SceneNodeTextInput : public SceneNode
{
public:
  SceneNodeTextInput(State<std::string> *state) : state_(state) {}
  void setState(State<std::string> *state) { state_ = state; }
  State<std::string> *state() const { return state_; }

protected:
  State<std::string> *state_;
};

#endif // DECLARA_TEXTINPUT_HPP
