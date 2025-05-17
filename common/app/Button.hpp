#ifndef DECLARA_BUTTON_HPP
#define DECLARA_BUTTON_HPP

#include <string>
#include "core/State.hpp"
#include "core/PlatformContext.hpp"
#include "core/SceneComponent.hpp"
#include "core/SceneNode.hpp"
#include "core/util/SceneNodeAttachScope.hpp"

// グローバル定数State（常に有効なボタン用）
static State<bool> BUTTON_DEFAULT_ENABLED(true);

class SceneNodeButton : public SceneNode
{
public:
  SceneNodeButton() : text_(""), enabled_(&BUTTON_DEFAULT_ENABLED)
  {
    SceneNodeAttachScope::autoAttach(this);
  }

  SceneNodeButton *setEnabled(State<bool> *state)
  {
    enabled_ = state;
    return this;
  }
  SceneNodeButton *setText(const std::string &text)
  {
    text_ = text;
    return this;
  }

  // ボタンのクリックイベント（購読側がbind/bindDeferで副作用を登録）
  EmitterState clickEvent;

  const std::string &text() const { return text_; }
  State<bool> *enabled() const { return enabled_; }

protected:
  std::string text_;
  State<bool> *enabled_;
};

#endif // DECLARA_BUTTON_HPP
