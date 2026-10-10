#ifndef LOKA_TEST_SUPPORT_CONTRAST_RATIO_HPP
#define LOKA_TEST_SUPPORT_CONTRAST_RATIO_HPP

#include <cmath>

namespace loka_test
{
  /** WCAG 2.x sRGB components are normalized to [0, 1]. Test-only arithmetic. */
  inline double LinearSrgb(double component)
  {
    return component <= 0.04045 ? component / 12.92
                               : std::pow((component + 0.055) / 1.055, 2.4);
  }
  inline double RelativeLuminance(double red, double green, double blue)
  {
    return 0.2126 * LinearSrgb(red) + 0.7152 * LinearSrgb(green) + 0.0722 * LinearSrgb(blue);
  }
  inline double ContrastRatio(double firstLuminance, double secondLuminance)
  {
    const double lighter = firstLuminance > secondLuminance ? firstLuminance : secondLuminance;
    const double darker = firstLuminance < secondLuminance ? firstLuminance : secondLuminance;
    return (lighter + 0.05) / (darker + 0.05);
  }
}

#endif
