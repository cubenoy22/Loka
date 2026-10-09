#include "ToolboxScenePlatformController.hpp"
#include "ToolboxLayoutMetrics.hpp"
#include "app/nodes/controls/Button.hpp"
#include "context/ToolboxLayoutUtil.hpp"
#include <climits>

bool ToolboxScenePlatformController::measurePushButtonNaturalWidth(const loka::core::String &label,
                                                                   short &width) const
{
  short text = 0;
  ToolboxTextMeasureScope scope(*this);
  if (!scope.measure(label, text))
    return false;
  const long total = static_cast<long>(text) + 2 * ToolboxLayoutMetrics::kPushButtonTitleInset;
  if (total <= 0 || total > SHRT_MAX)
    return false;
  width = static_cast<short>(total);
  return true;
}

short ToolboxScenePlatformController::measurePushButtonNaturalWidth(const loka::core::String &label) const
{
  short width = 0;
  if (this->measurePushButtonNaturalWidth(label, width))
    return width;
  // A refused measurement keeps the standalone layout's inset-only width.
  return static_cast<short>(2 * ToolboxLayoutMetrics::kPushButtonTitleInset);
}

bool ToolboxScenePlatformController::queryNaturalWidth(loka::app::scene::Node *child, short &width) const
{
  loka::app::ButtonNode *button = child ? child->asButtonNode() : 0;
  if (!button)
    return false;
  const loka::core::String label =
      button->props.text_ ? button->props.text_->get() : loka::core::String::Literal("Button");
  // A refused measurement declines, so the Row keeps a shared seat.
  return this->measurePushButtonNaturalWidth(label, width);
}
