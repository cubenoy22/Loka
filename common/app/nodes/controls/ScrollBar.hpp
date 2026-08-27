#ifndef LOKA_APP_SCROLL_BAR_HPP
#define LOKA_APP_SCROLL_BAR_HPP

#include "core/State.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/state/NodeState.hpp"

namespace loka
{
  namespace app
  {
    /** Orientation is declared, never inferred from the projected rect. A
        scroll bar is 16px thick on its cross axis, so a short one is an
        ambiguous shape and a geometric guess would silently pick the wrong
        CDEF variant. Vertical is the default because that is the axis a
        document window scrolls without being asked. */
    enum ScrollBarOrientation
    {
      SCROLL_BAR_VERTICAL = 0,
      SCROLL_BAR_HORIZONTAL = 1
    };

    enum
    {
      /** Classic's fixed scroll bar thickness. The cross-axis extent is a
          constant of the control rather than a layout negotiation; only the
          length along the scrolling axis comes from the layout pass. */
      SCROLL_BAR_THICKNESS = 16
    };

    /** True when the range admits more than one position. min == max is a
        legal scene -- a scroll bar over an empty document exists -- so the
        arms present it inactive instead of refusing it. */
    inline bool ScrollBarIsScrollable(int minimum, int maximum)
    {
      return maximum > minimum;
    }

    /** Presentation-side clamp for a bound value outside [min, max]. This is
        the same shape PopupMenu already uses for an out-of-range
        selectedIndex: the view clamps what it shows and leaves the bound
        State untouched. Writing a corrected value back would make merely
        composing a scene mutate application state. */
    inline int ScrollBarClampValue(int value, int minimum, int maximum)
    {
      if (maximum <= minimum)
      {
        return minimum;
      }
      if (value < minimum)
      {
        return minimum;
      }
      if (value > maximum)
      {
        return maximum;
      }
      return value;
    }

    struct ScrollBarTypeTag
    {
    };

    class ScrollBarNode;

    struct ScrollBarProps : public loka::app::scene::NodePropsBase<ScrollBarProps>
    {
      typedef ScrollBarTypeTag TypeTag;
      typedef ScrollBarNode NodeType;
      /** Two-way binding. Only settled values ever reach it -- see the arms:
          the write happens after the tracking loop returns, never during. */
      loka::core::MutableState<int> *value_;
      /** Static like PopupMenu's items: a range change is a recompose, not a
          State notification, so the range cannot drift out from under a
          tracking loop that is already running.

          The Toolbox arm carries the range in the control's 16-bit fields,
          so values outside a short are a platform limit there, not a
          framework rule -- declare ranges that fit. */
      int min_;
      int max_;
      ScrollBarOrientation orientation_;
      int lineStep_;
      int pageStep_;
      loka::core::State<bool> *enabled_;
      loka::core::EmitterState *onChange_;
      int controlTag_;

      ScrollBarProps()
          : value_(0),
            min_(0),
            max_(0),
            orientation_(SCROLL_BAR_VERTICAL),
            lineStep_(1),
            pageStep_(1),
            enabled_(0),
            onChange_(0),
            controlTag_(0)
      {
      }

      explicit ScrollBarProps(loka::core::MutableState<int> *value)
          : value_(value),
            min_(0),
            max_(0),
            orientation_(SCROLL_BAR_VERTICAL),
            lineStep_(1),
            pageStep_(1),
            enabled_(0),
            onChange_(0),
            controlTag_(0)
      {
      }

      explicit ScrollBarProps(const loka::app::scene::NodeState<int> &value)
          : value_(value.dangerouslyMutableState()),
            min_(0),
            max_(0),
            orientation_(SCROLL_BAR_VERTICAL),
            lineStep_(1),
            pageStep_(1),
            enabled_(0),
            onChange_(0),
            controlTag_(0)
      {
      }

      ScrollBarProps &value(loka::core::MutableState<int> *value)
      {
        this->value_ = value;
        return *this;
      }

      ScrollBarProps &value(const loka::app::scene::NodeState<int> &value)
      {
        this->value_ = value.dangerouslyMutableState();
        return *this;
      }

      ScrollBarProps &range(int minimum, int maximum)
      {
        this->min_ = minimum;
        this->max_ = maximum;
        return *this;
      }

      ScrollBarProps &orientation(ScrollBarOrientation orientation)
      {
        this->orientation_ = orientation;
        return *this;
      }

      ScrollBarProps &vertical()
      {
        this->orientation_ = SCROLL_BAR_VERTICAL;
        return *this;
      }

      ScrollBarProps &horizontal()
      {
        this->orientation_ = SCROLL_BAR_HORIZONTAL;
        return *this;
      }

      ScrollBarProps &lineStep(int step)
      {
        this->lineStep_ = step;
        return *this;
      }

      ScrollBarProps &pageStep(int step)
      {
        this->pageStep_ = step;
        return *this;
      }

      ScrollBarProps &enabled(loka::core::State<bool> *enabled)
      {
        this->enabled_ = enabled;
        return *this;
      }

      ScrollBarProps &onChange(loka::core::EmitterState *onChange)
      {
        this->onChange_ = onChange;
        return *this;
      }

      ScrollBarProps &controlTag(int tag)
      {
        this->controlTag_ = tag;
        return *this;
      }

      bool operator<(const loka::app::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const ScrollBarProps &other = static_cast<const ScrollBarProps &>(rhs);
        if (controlTag_ != other.controlTag_)
          return controlTag_ < other.controlTag_;
        if (min_ != other.min_)
          return min_ < other.min_;
        if (max_ != other.max_)
          return max_ < other.max_;
        if (orientation_ != other.orientation_)
          return orientation_ < other.orientation_;
        if (lineStep_ != other.lineStep_)
          return lineStep_ < other.lineStep_;
        if (pageStep_ != other.pageStep_)
          return pageStep_ < other.pageStep_;
        if (value_ != other.value_)
          return value_ < other.value_;
        // A recompose that swaps only the handler must not be "equivalent",
        // or the retained fast path keeps gesturing at the old one.
        if (onChange_ != other.onChange_)
          return onChange_ < other.onChange_;
        return enabled_ < other.enabled_;
      }
    };

    class ScrollBarNode : public loka::app::scene::Node, public loka::app::scene::IProjectedLayoutNode
    {
    public:
      typedef ScrollBarTypeTag TypeTag;
      ScrollBarProps props;
      ScrollBarNode(const ScrollBarProps &p)
          : props(p)
      {
      }
      virtual loka::app::scene::NodeKind kind() const
      {
        return loka::app::scene::NODE_KIND_SCROLL_BAR;
      }
      virtual loka::app::scene::IProjectedLayoutNode *asProjectedLayoutNode()
      {
        return this;
      }
      virtual const void *nodeTypeKey() const
      {
        return loka::app::scene::NodeTypeToken<ScrollBarNode>();
      }
      virtual ScrollBarNode *asScrollBarNode()
      {
        return this;
      }
      virtual short layoutProjected(loka::app::scene::IPlatformController *controller,
                                    loka::app::scene::LayoutState &state)
      {
        if (!controller)
        {
          return state.y;
        }
        if (!loka::app::scene::PrepareProjectedLayout(controller, this, state))
        {
          return state.y;
        }
        return loka::app::scene::Node::layout(controller, state);
      }
      virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
      {
        if (this->props.value_)
        {
          registrar.markDirtyOnChange(this->props.value_, loka::app::scene::NODE_DIRTY_PROPS);
        }
        if (this->props.enabled_)
        {
          registrar.markDirtyOnChange(this->props.enabled_, loka::app::scene::NODE_DIRTY_PROPS);
        }
      }

      /** The value the control should show right now. Reading through this
          keeps every arm clamping identically. */
      int displayValue() const
      {
        const int bound = this->props.value_ ? this->props.value_->get() : this->props.min_;
        return ScrollBarClampValue(bound, this->props.min_, this->props.max_);
      }

      /** Classic presents an unscrollable or disabled bar with hilite 255;
          the two reasons collapse into one presentation question here so no
          arm has to remember both. */
      bool isActive() const
      {
        if (this->props.enabled_ && !this->props.enabled_->get())
        {
          return false;
        }
        return ScrollBarIsScrollable(this->props.min_, this->props.max_);
      }
    };

    struct ScrollBarDefinition : public loka::app::scene::NodeDefinition<ScrollBarProps, ScrollBarNode>,
                                 public loka::app::scene::TestIdDslMixin<ScrollBarDefinition>
    {
      ScrollBarDefinition()
          : loka::app::scene::NodeDefinition<ScrollBarProps, ScrollBarNode>()
      {
      }
      ScrollBarDefinition(const ScrollBarProps &p)
          : loka::app::scene::NodeDefinition<ScrollBarProps, ScrollBarNode>(p)
      {
      }
      explicit ScrollBarDefinition(loka::core::MutableState<int> *value)
          : loka::app::scene::NodeDefinition<ScrollBarProps, ScrollBarNode>(ScrollBarProps(value))
      {
      }
      explicit ScrollBarDefinition(const loka::app::scene::NodeState<int> &value)
          : loka::app::scene::NodeDefinition<ScrollBarProps, ScrollBarNode>(ScrollBarProps(value))
      {
      }

      ScrollBarDefinition &value(loka::core::MutableState<int> *value)
      {
        this->props.value(value);
        return *this;
      }

      ScrollBarDefinition &value(const loka::app::scene::NodeState<int> &value)
      {
        this->props.value(value);
        return *this;
      }

      ScrollBarDefinition &range(int minimum, int maximum)
      {
        this->props.range(minimum, maximum);
        return *this;
      }

      ScrollBarDefinition &orientation(ScrollBarOrientation orientation)
      {
        this->props.orientation(orientation);
        return *this;
      }

      ScrollBarDefinition &vertical()
      {
        this->props.vertical();
        return *this;
      }

      ScrollBarDefinition &horizontal()
      {
        this->props.horizontal();
        return *this;
      }

      ScrollBarDefinition &lineStep(int step)
      {
        this->props.lineStep(step);
        return *this;
      }

      ScrollBarDefinition &pageStep(int step)
      {
        this->props.pageStep(step);
        return *this;
      }

      ScrollBarDefinition &enabled(loka::core::State<bool> *enabled)
      {
        this->props.enabled(enabled);
        return *this;
      }

      ScrollBarDefinition &onChange(loka::core::EmitterState *onChange)
      {
        this->props.onChange(onChange);
        return *this;
      }

      ScrollBarDefinition &controlTag(int tag)
      {
        this->props.controlTag(tag);
        return *this;
      }
      using loka::app::scene::NodeDefinition<ScrollBarProps, ScrollBarNode>::create;
    };

    typedef ScrollBarDefinition ScrollBar;
  } // namespace app
} // namespace loka

#endif // LOKA_APP_SCROLL_BAR_HPP
