#include "context/ToolboxLayoutUtil.hpp"

#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "platform/StringUTF8.hpp"
#include <Fonts.h>
#include <cstring>
#include <string>

namespace
{
  short ResolvedFontSize(short size)
  {
    return size == 0 ? GetDefFontSize() : size;
  }
}

bool ToolboxBuildPascalText(const loka::core::String &value, Str255 text)
{
  std::string utf8;
  if (!loka::platform::CollectUtf8(value, utf8))
  {
    text[0] = 0;
    return false;
  }
  std::size_t length = utf8.size();
  if (length > 255)
  {
    length = 255;
  }
  text[0] = static_cast<unsigned char>(length);
  if (length > 0)
  {
    std::memcpy(text + 1, utf8.data(), length);
  }
  return true;
}

ToolboxTextMeasureScope::ToolboxTextMeasureScope(
    const ToolboxScenePlatformController &controller,
    const ToolboxTextFontDescriptor &descriptor)
    : previousPort_(0),
      measurePort_(controller.window_ ? reinterpret_cast<GrafPtr>(controller.window_->window()) : 0),
      previousFont_(this->measurePort_ ? this->measurePort_->txFont : 0),
      previousSize_(this->measurePort_ ? this->measurePort_->txSize : 0),
      previousFace_(this->measurePort_ ? this->measurePort_->txFace : 0),
      busy_(this->measurePort_
                && ResolvedFontSize(descriptor.size(this->previousSize_)) != ResolvedFontSize(this->previousSize_)
            ? controller.cursorOwner() : 0)
{
  GetPort(&this->previousPort_);
  if (!this->measurePort_)
  {
    return;
  }

  SetPort(this->measurePort_);
  const short font = descriptor.font(this->previousFont_);
  TextFont(font == 0 ? GetSysFont() : font);
  TextSize(ResolvedFontSize(descriptor.size(this->previousSize_)));
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
