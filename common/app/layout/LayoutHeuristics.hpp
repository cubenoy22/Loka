#ifndef LOKA_APP_LAYOUT_HEURISTICS_HPP
#define LOKA_APP_LAYOUT_HEURISTICS_HPP

#include "app/ImageView.hpp"
#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      inline int clampToAvailable(int value, int available)
      {
        if (value < 0)
        {
          return 0;
        }
        if (available >= 0 && value > available)
        {
          return available;
        }
        return value;
      }

      inline int preferredChildWidthForColumn(loka::app::scene::Node *child, int availableWidth)
      {
        if (!child)
        {
          return clampToAvailable(availableWidth, availableWidth);
        }
        if (loka::app::ImageViewNode *image = child->asImageViewNode())
        {
          if (image->props.width_ > 0)
          {
            return clampToAvailable(image->props.width_, availableWidth);
          }
        }
        return clampToAvailable(availableWidth, availableWidth);
      }

      inline int remainingChildHeightForColumn(int parentHeight, int parentStartY, int currentY)
      {
        if (parentHeight <= 0)
        {
          return parentHeight;
        }
        const int usedHeight = currentY - parentStartY;
        int remainingHeight = parentHeight - usedHeight;
        if (remainingHeight < 0)
        {
          remainingHeight = 0;
        }
        return remainingHeight;
      }

      inline int preferredChildHeightForRow(loka::app::scene::Node *child,
                                            int fallbackHeight,
                                            int buttonHeight,
                                            int editTextHeight,
                                            int popupMenuHeight,
                                            int textHeight,
                                            int imageFallbackHeight)
      {
        if (!child)
        {
          return fallbackHeight;
        }
        if (child->asButtonNode())
        {
          return buttonHeight;
        }
        if (child->asEditTextNode())
        {
          return editTextHeight;
        }
        if (child->asPopupMenuNode())
        {
          return popupMenuHeight;
        }
        if (child->asTextNode())
        {
          return textHeight;
        }
        if (loka::app::ImageViewNode *image = child->asImageViewNode())
        {
          if (image->props.height_ > 0)
          {
            return image->props.height_;
          }
          if (fallbackHeight > 0)
          {
            return fallbackHeight;
          }
          return imageFallbackHeight;
        }
        return fallbackHeight;
      }
    }
  }
}

#endif // LOKA_APP_LAYOUT_HEURISTICS_HPP
