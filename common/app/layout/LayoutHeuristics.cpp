#include "app/layout/LayoutHeuristics.hpp"

#include "app/nodes/nestable/Fragment.hpp"
#include "dsl/composition/CompositionList.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      int preferredChildWidthForRow(loka::app::scene::Node *child)
      {
        if (!child)
        {
          return -1;
        }
        if (child->propsTypeId() == loka::app::FragmentProps::staticTypeId())
        {
          loka::app::scene::INestable *fragment = child->asNestable();
          if (!fragment || fragment->childrenCount() == 0)
          {
            return 0;
          }
          if (fragment->childrenCount() == 1)
          {
            return preferredChildWidthForRow(fragment->childrenHead());
          }
          return -1;
        }
        if (loka::app::ImageViewNode *image = child->asImageViewNode())
        {
          if (image->props.width_ > 0)
          {
            return image->props.width_;
          }
        }
        if (loka::app::BoxNode *box = child->asBoxNode())
        {
          if (box->props.width > 0)
          {
            return box->props.width;
          }
        }
        return -1;
      }

      RowWidthConsultation::RowWidthConsultation(loka::app::scene::Node *childrenHead,
                                                 size_t childCount,
                                                 int availableWidth,
                                                 int gap)
          : baseFlexWidth_(0),
            flexRemainder_(0),
            liveSeatsSeen_(0)
      {
        int fixedWidthTotal = 0;
        int liveSeats = 0;
        int flexSeats = 0;
        loka::dsl::CompositionCursor<loka::app::scene::Node> consult(childrenHead, childCount);
        for (loka::app::scene::Node *child = consult.next(); child; child = consult.next())
        {
          const int preferredWidth = preferredChildWidthForRow(child);
          if (preferredWidth == 0)
          {
            continue;
          }
          ++liveSeats;
          if (preferredWidth < 0)
          {
            ++flexSeats;
          }
          else
          {
            fixedWidthTotal += preferredWidth;
          }
        }
        const int spacingTotal = gap * (liveSeats > 0 ? liveSeats - 1 : 0);
        int remainingWidth = availableWidth - fixedWidthTotal - spacingTotal;
        if (remainingWidth < 0)
        {
          remainingWidth = 0;
        }
        if (flexSeats > 0)
        {
          this->baseFlexWidth_ = remainingWidth / flexSeats;
          this->flexRemainder_ = remainingWidth - this->baseFlexWidth_ * flexSeats;
        }
      }

      RowChildWidth RowWidthConsultation::next(loka::app::scene::Node *child)
      {
        const int preferredWidth = preferredChildWidthForRow(child);
        if (preferredWidth == 0)
        {
          return RowChildWidth(0, false, false);
        }
        int width = preferredWidth;
        if (preferredWidth < 0)
        {
          width = this->baseFlexWidth_;
          if (this->flexRemainder_ > 0)
          {
            ++width;
            --this->flexRemainder_;
          }
        }
        const bool gapBefore = this->liveSeatsSeen_ > 0;
        ++this->liveSeatsSeen_;
        return RowChildWidth(width, true, gapBefore);
      }
    } // namespace layout
  } // namespace app
} // namespace loka
