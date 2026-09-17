#ifndef LOKA_TOOLBOX_LAYOUT_UTIL_HPP
#define LOKA_TOOLBOX_LAYOUT_UTIL_HPP

#include "core/String.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;

/** The only font descriptor admitted by PR 0: preserve the window port font. */
class ToolboxTextFontDescriptor
{
public:
  ToolboxTextFontDescriptor() {}

  short font(short portFont) const
  {
    return portFont;
  }
  short size(short portSize) const
  {
    return portSize;
  }
  Style face(Style portFace) const
  {
    return portFace;
  }
};

/** Copies the same capped Pascal bytes consumed by Toolbox DrawString. */
bool ToolboxBuildPascalText(const loka::core::String &value, Str255 text);

/** Window-bound text measurement transaction.

    The controller supplies the window owner. The scope restores both the
    window port's font state and whichever GrafPort the caller had selected. */
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
};

#endif // LOKA_TOOLBOX_LAYOUT_UTIL_HPP
