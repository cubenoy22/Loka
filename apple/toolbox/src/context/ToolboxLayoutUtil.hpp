#ifndef LOKA_TOOLBOX_LAYOUT_UTIL_HPP
#define LOKA_TOOLBOX_LAYOUT_UTIL_HPP

#include "core/String.hpp"
#include "app/style/Style.hpp"
#include "ToolboxApp.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;

/** Resolved Text style projected onto the owning port's font family.
    Unset fields inherit the port; explicit face fields replace only their bit. */
class ToolboxTextFontDescriptor
{
public:
  ToolboxTextFontDescriptor() : style_() {}
  explicit ToolboxTextFontDescriptor(const loka::app::TextStyle &style) : style_(style) {}

  short font(short portFont) const
  {
    return portFont;
  }
  short size(short portSize) const
  {
    return this->style_.hasFontSize_
               ? static_cast<short>(loka::app::SizeOf(this->style_.fontSize_).fontSize_)
               : portSize;
  }
  Style face(Style portFace) const
  {
    if (this->style_.hasWeight_)
      portFace = static_cast<Style>((portFace & ~bold)
          | (this->style_.weight_ == loka::app::TEXT_WEIGHT_BOLD ? bold : 0));
    if (this->style_.hasItalic_)
      portFace = static_cast<Style>((portFace & ~italic) | (this->style_.italic_ ? italic : 0));
    return portFace;
  }

private:
  loka::app::TextStyle style_;
};

/** Copies the same capped Pascal bytes consumed by Toolbox DrawString. */
bool ToolboxBuildPascalText(const loka::core::String &value, Str255 text);

/** Window-bound text measurement and painting transaction.

    The controller supplies the window owner. The scope restores both the
    window port's font state and whichever GrafPort the caller had selected.
    Non-default sizes borrow the app cursor through selection, measurement,
    drawing and restoration, on every transaction regardless of font family. */
class ToolboxTextMeasureScope
{
public:
  ToolboxTextMeasureScope(
      const ToolboxScenePlatformController &controller,
      const ToolboxTextFontDescriptor &descriptor = ToolboxTextFontDescriptor());
  ~ToolboxTextMeasureScope();
  short measure(const loka::core::String &value) const;

private:
  ToolboxTextMeasureScope(const ToolboxTextMeasureScope &);
  ToolboxTextMeasureScope &operator=(const ToolboxTextMeasureScope &);

  GrafPtr previousPort_;
  GrafPtr measurePort_;
  short previousFont_;
  short previousSize_;
  Style previousFace_;
  BusyScope busy_;
};

#endif // LOKA_TOOLBOX_LAYOUT_UTIL_HPP
