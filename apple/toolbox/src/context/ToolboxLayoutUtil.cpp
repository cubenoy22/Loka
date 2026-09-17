#include "context/ToolboxLayoutUtil.hpp"

#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "platform/ToolboxMacRoman.hpp"

bool ToolboxBuildPascalText(const loka::core::String &value, Str255 text)
{
  std::size_t length = 0;
  const loka::toolbox::ToolboxMacRomanResult result =
      loka::toolbox::CopyStringToMacRoman(value, text + 1, 255, length);
  if (result == loka::toolbox::TOOLBOX_MAC_ROMAN_INVALID)
  {
    text[0] = 0;
    return false;
  }
  text[0] = static_cast<unsigned char>(length);
  return true;
}

ToolboxTextMeasureScope::ToolboxTextMeasureScope(
    const ToolboxScenePlatformController &controller,
    const ToolboxTextFontDescriptor &descriptor)
    : previousPort_(0),
      measurePort_(0),
      previousFont_(0),
      previousSize_(0),
      previousFace_(0)
{
  GetPort(&this->previousPort_);
  ToolboxWindow *window = controller.window_;
  if (!window || !window->window())
  {
    return;
  }

  SetPort(window->window());
  GetPort(&this->measurePort_);
  if (!this->measurePort_)
  {
    SetPort(this->previousPort_);
    return;
  }

  this->previousFont_ = this->measurePort_->txFont;
  this->previousSize_ = this->measurePort_->txSize;
  this->previousFace_ = this->measurePort_->txFace;
  TextFont(descriptor.font(this->previousFont_));
  TextSize(descriptor.size(this->previousSize_));
  TextFace(descriptor.face(this->previousFace_));
}

ToolboxTextMeasureScope::~ToolboxTextMeasureScope()
{
  if (this->measurePort_)
  {
    SetPort(this->measurePort_);
    TextFont(this->previousFont_);
    TextSize(this->previousSize_);
    TextFace(this->previousFace_);
  }
  SetPort(this->previousPort_);
}

short ToolboxTextMeasureScope::measure(const loka::core::String &value) const
{
  if (!this->measurePort_)
  {
    return 0;
  }
  Str255 text;
  if (!ToolboxBuildPascalText(value, text))
  {
    return 0;
  }
  return StringWidth(text);
}
