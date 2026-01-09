#ifndef LOKA_TOOLBOX_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_TEXT_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/Text.hpp"
#include "loka/core/String.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;

class ToolboxTextContext : public declara::core::scene::NativeNodeContext
{
public:
  explicit ToolboxTextContext(declara::app::TextNode *node);
  virtual ~ToolboxTextContext();

  void updateData(declara::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect, short textX, short textY);
  void draw(ToolboxScenePlatformController *controller);

private:
  declara::app::TextNode *node_;
  Rect rect_;
  short textX_;
  short textY_;
  declara::core::State<loka::core::String> *text_;
};

#endif // LOKA_TOOLBOX_TEXT_CONTEXT_HPP
