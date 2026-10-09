#include "ToolboxScenePlatformController.hpp"
#include "ToolboxLayoutMetrics.hpp"
#include "app/nodes/controls/Button.hpp"

short ToolboxScenePlatformController::measurePushButtonNaturalWidth(const loka::core::String &label) const
{
  return static_cast<short>(this->measureTextWidth(label) + 2 * ToolboxLayoutMetrics::kPushButtonTitleInset);
}

bool ToolboxScenePlatformController::queryNaturalWidth(loka::app::scene::Node *child, short &width) const
{
  loka::app::ButtonNode *button = child ? child->asButtonNode() : 0;
  if (!button)
    return false;
  const loka::core::String label =
      button->props.text_ ? button->props.text_->get() : loka::core::String::Literal("Button");
  width = this->measurePushButtonNaturalWidth(label);
  return width > 0;
}
