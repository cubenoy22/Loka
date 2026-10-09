#ifndef LOKA_APP_NODES_CONTROLS_RIBBON_HPP
#define LOKA_APP_NODES_CONTROLS_RIBBON_HPP

#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include <cassert>

namespace loka
{
  namespace app
  {
    struct RibbonControlDefinition;

    /** One native push button in a left-to-right RibbonControl, using its rail's
        natural width unless width(n) declares a fixed seat. The emitter is borrowed;
        null does nothing.
        Items form the closed ribbon vocabulary for future native projections;
        enabled, icons and identifier commands are later work. */
    struct RibbonItemDefinition
    {
      explicit RibbonItemDefinition(const char *text)
          : title_(loka::core::String::Literal(text)),
            onClick_(0),
            width_()
      {
      }
      RibbonItemDefinition &onClick(loka::core::EmitterState *emitter)
      {
        this->onClick_ = emitter;
        return *this;
      }
      /** Overrides the seat width; it must be positive. */
      RibbonItemDefinition &width(short n)
      {
        assert(n > 0);
        this->width_ = DeclaredWidth(n);
        return *this;
      }

    private:
      // Read only by RibbonControlDefinition when it expands the item.
      friend struct RibbonControlDefinition;
      loka::core::String title_;
      loka::core::EmitterState *onClick_;
      /** Optional fixed seat; the numeric payload is read only in FIXED mode. */
      class DeclaredWidth
      {
      public:
        DeclaredWidth() : kind_(NATURAL), value_(0) {}
        explicit DeclaredWidth(short value) : kind_(FIXED), value_(value) {}
        bool query(short &value) const
        {
          if (this->kind_ == NATURAL)
            return false;
          value = this->value_;
          return true;
        }
      private:
        enum Kind { NATURAL, FIXED };
        Kind kind_;
        short value_;
      };
      DeclaredWidth width_;
    };
    typedef RibbonItemDefinition RibbonItem;

    /** A left-to-right row of native push buttons using rail-measured natural
        widths or the item's explicit width(n). A declining rail shares the remaining
        width among undeclared seats. Only RibbonItems may
        be appended, preserving a closed declaration for future native projections.
        Enabled, icons and identifier commands are later work. */
    struct RibbonControlDefinition
        : public scene::NestableNodeDefinition<StackProps, StackNode, RibbonControlDefinition>,
          public scene::TestIdDslMixin<RibbonControlDefinition>
    {
      typedef scene::NestableNodeDefinition<StackProps, StackNode, RibbonControlDefinition> BaseType;
      RibbonControlDefinition()
          : BaseType(StackProps(STACK_AXIS_ROW))
      {
        this->props.rowUndeclaredWidth_ = ROW_UNDECLARED_WIDTH_NATURAL;
      }
      RibbonControlDefinition(const RibbonControlDefinition &other)
          : BaseType(other)
      {
      }

      // Declaring this operator hides the generic node operator<< of the base,
      // so a RibbonControl accepts RibbonItems only.
      RibbonControlDefinition &operator<<(const RibbonItemDefinition &item)
      {
        short fixedWidth = 0;
        if (item.width_.query(fixedWidth))
          BaseType::operator<<(Box().size(fixedWidth, 0)
                               << Button(ButtonProps().text(item.title_).onClick(item.onClick_)));
        else
          BaseType::operator<<(Button(ButtonProps().text(item.title_).onClick(item.onClick_)));
        return *this;
      }
    };
    typedef RibbonControlDefinition RibbonControl;
  } // namespace app
} // namespace loka

#endif // LOKA_APP_NODES_CONTROLS_RIBBON_HPP
