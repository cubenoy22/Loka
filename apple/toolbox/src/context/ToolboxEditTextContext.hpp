#ifndef LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/EditText.hpp"
#include "loka/core/String.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;

class ToolboxEditTextContext : public declara::core::scene::NativeNodeContext
{
public:
  explicit ToolboxEditTextContext(declara::app::EditTextNode *node);
  virtual ~ToolboxEditTextContext();

  void updateData(declara::core::State<loka::core::String> *text);
  void updateRect(const Rect &outerRect, const Rect &textRect, short textX, short textY);
  void draw(ToolboxScenePlatformController *controller);

private:
  declara::app::EditTextNode *node_;
  Rect rect_;
  Rect textRect_;
  short textX_;
  short textY_;
  declara::core::State<loka::core::String> *text_;
};

#endif // LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
