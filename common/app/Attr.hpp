#ifndef LOKA_APP_ATTR_HPP
#define LOKA_APP_ATTR_HPP

#include "loka/core/State.hpp"

namespace loka
{
  namespace app
  {
    enum TextWeight
    {
      TEXT_WEIGHT_NORMAL = 0,
      TEXT_WEIGHT_BOLD = 1
    };

    enum ImageFit
    {
      IMAGE_FIT_NONE = 0,
      IMAGE_FIT_CONTAIN = 1,
      IMAGE_FIT_COVER = 2,
      IMAGE_FIT_STRETCH = 3
    };

    enum ImageViewSizePolicy
    {
      IMAGE_VIEW_SIZE_AUTO = 0,
      IMAGE_VIEW_SIZE_FILL_PARENT = 1,
      IMAGE_VIEW_SIZE_INTRINSIC = 2
    };

    struct TextAttr
    {
      TextAttr()
          : hasFontSizeValue_(false),
            fontSizeValue_(0),
            fontSizeState_(0),
            hasWeightValue_(false),
            weightValue_(TEXT_WEIGHT_NORMAL)
      {
      }

      TextAttr &fontSize(int value)
      {
        this->hasFontSizeValue_ = true;
        this->fontSizeValue_ = value;
        this->fontSizeState_ = 0;
        return *this;
      }

      TextAttr &fontSize(loka::core::State<int> *state)
      {
        this->hasFontSizeValue_ = false;
        this->fontSizeState_ = state;
        return *this;
      }

      TextAttr &weight(TextWeight value)
      {
        this->hasWeightValue_ = true;
        this->weightValue_ = value;
        return *this;
      }

      bool operator==(const TextAttr &other) const
      {
        return this->hasFontSizeValue_ == other.hasFontSizeValue_ &&
               this->fontSizeValue_ == other.fontSizeValue_ &&
               this->fontSizeState_ == other.fontSizeState_ &&
               this->hasWeightValue_ == other.hasWeightValue_ &&
               this->weightValue_ == other.weightValue_;
      }

      bool operator<(const TextAttr &other) const
      {
        if (this->hasFontSizeValue_ != other.hasFontSizeValue_)
          return this->hasFontSizeValue_ < other.hasFontSizeValue_;
        if (this->fontSizeValue_ != other.fontSizeValue_)
          return this->fontSizeValue_ < other.fontSizeValue_;
        if (this->fontSizeState_ != other.fontSizeState_)
          return this->fontSizeState_ < other.fontSizeState_;
        if (this->hasWeightValue_ != other.hasWeightValue_)
          return this->hasWeightValue_ < other.hasWeightValue_;
        return this->weightValue_ < other.weightValue_;
      }

      bool hasFontSizeValue_;
      int fontSizeValue_;
      loka::core::State<int> *fontSizeState_;
      bool hasWeightValue_;
      TextWeight weightValue_;
    };

    struct ImageViewAttr
    {
      ImageViewAttr()
          : hasFitValue_(false),
            fitValue_(IMAGE_FIT_NONE),
            hasSizePolicyValue_(false),
            sizePolicyValue_(IMAGE_VIEW_SIZE_AUTO)
      {
      }

      ImageViewAttr &fit(ImageFit value)
      {
        this->hasFitValue_ = true;
        this->fitValue_ = value;
        return *this;
      }

      ImageViewAttr &sizePolicy(ImageViewSizePolicy value)
      {
        this->hasSizePolicyValue_ = true;
        this->sizePolicyValue_ = value;
        return *this;
      }

      bool operator==(const ImageViewAttr &other) const
      {
        return this->hasFitValue_ == other.hasFitValue_ &&
               this->fitValue_ == other.fitValue_ &&
               this->hasSizePolicyValue_ == other.hasSizePolicyValue_ &&
               this->sizePolicyValue_ == other.sizePolicyValue_;
      }

      bool operator<(const ImageViewAttr &other) const
      {
        if (this->hasFitValue_ != other.hasFitValue_)
          return this->hasFitValue_ < other.hasFitValue_;
        if (this->fitValue_ != other.fitValue_)
          return this->fitValue_ < other.fitValue_;
        if (this->hasSizePolicyValue_ != other.hasSizePolicyValue_)
          return this->hasSizePolicyValue_ < other.hasSizePolicyValue_;
        return this->sizePolicyValue_ < other.sizePolicyValue_;
      }

      bool hasFitValue_;
      ImageFit fitValue_;
      bool hasSizePolicyValue_;
      ImageViewSizePolicy sizePolicyValue_;
    };

    // Backward-compatible alias. Keep until all call sites migrate.
    typedef ImageViewAttr ImageAttr;

    struct MenuItemAttr
    {
      MenuItemAttr()
          : hasDisabledValue_(false),
            disabledValue_(false),
            disabledState_(0),
            hasVisibleValue_(false),
            visibleValue_(true),
            visibleState_(0)
      {
      }

      MenuItemAttr &disabled(bool value)
      {
        this->hasDisabledValue_ = true;
        this->disabledValue_ = value;
        this->disabledState_ = 0;
        return *this;
      }

      MenuItemAttr &disabled(loka::core::State<bool> *state)
      {
        this->hasDisabledValue_ = false;
        this->disabledState_ = state;
        return *this;
      }

      MenuItemAttr &visible(bool value)
      {
        this->hasVisibleValue_ = true;
        this->visibleValue_ = value;
        this->visibleState_ = 0;
        return *this;
      }

      MenuItemAttr &visible(loka::core::State<bool> *state)
      {
        this->hasVisibleValue_ = false;
        this->visibleState_ = state;
        return *this;
      }

      bool operator==(const MenuItemAttr &other) const
      {
        return this->hasDisabledValue_ == other.hasDisabledValue_ &&
               this->disabledValue_ == other.disabledValue_ &&
               this->disabledState_ == other.disabledState_ &&
               this->hasVisibleValue_ == other.hasVisibleValue_ &&
               this->visibleValue_ == other.visibleValue_ &&
               this->visibleState_ == other.visibleState_;
      }

      bool operator<(const MenuItemAttr &other) const
      {
        if (this->hasDisabledValue_ != other.hasDisabledValue_)
          return this->hasDisabledValue_ < other.hasDisabledValue_;
        if (this->disabledValue_ != other.disabledValue_)
          return this->disabledValue_ < other.disabledValue_;
        if (this->disabledState_ != other.disabledState_)
          return this->disabledState_ < other.disabledState_;
        if (this->hasVisibleValue_ != other.hasVisibleValue_)
          return this->hasVisibleValue_ < other.hasVisibleValue_;
        if (this->visibleValue_ != other.visibleValue_)
          return this->visibleValue_ < other.visibleValue_;
        return this->visibleState_ < other.visibleState_;
      }

      bool hasDisabledValue_;
      bool disabledValue_;
      loka::core::State<bool> *disabledState_;
      bool hasVisibleValue_;
      bool visibleValue_;
      loka::core::State<bool> *visibleState_;
    };
  } // namespace app
} // namespace loka

#endif // LOKA_APP_ATTR_HPP
