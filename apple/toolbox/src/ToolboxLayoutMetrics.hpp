#ifndef LOKA_TOOLBOX_LAYOUT_METRICS_HPP
#define LOKA_TOOLBOX_LAYOUT_METRICS_HPP

/** Toolbox-native geometry shared by layout projections and control painters. */
struct ToolboxLayoutMetrics
{
  static const short kDefaultLineHeight = 12;
  static const short kImageFallbackHeight = 80;
  /** Baseline controls paint from y - lineHeight + ascentInset to y + descent. */
  static const short kControlAscentInset = 2;
  static const short kControlDescent = 6;
  static const short kEditTextDescent = 8;
};

#endif // LOKA_TOOLBOX_LAYOUT_METRICS_HPP
