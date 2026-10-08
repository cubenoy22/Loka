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

    /** One native push button in a left-to-right RibbonControl, in a fixed-width
        seat (DEFAULT_WIDTH or width(n)). The emitter is borrowed; null does nothing.
        Items form the closed ribbon vocabulary for future native projections;
        enabled, icons and identifier commands are later work. */
    struct RibbonItemDefinition
    {
      static const short DEFAULT_WIDTH = 80;

      explicit RibbonItemDefinition(const char *text)
          : title_(loka::core::String::Literal(text)),
            onClick_(0),
            width_(DEFAULT_WIDTH)
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
        this->width_ = n;
        return *this;
      }

    private:
      // Read only by RibbonControlDefinition when it expands the item.
      friend struct RibbonControlDefinition;
      loka::core::String title_;
      loka::core::EmitterState *onClick_;
      short width_;
    };
    typedef RibbonItemDefinition RibbonItem;

    /** A left-to-right row of native push buttons, each in a fixed-width seat
        (RibbonItem::DEFAULT_WIDTH or the item's width(n)). Only RibbonItems may
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
      }
      RibbonControlDefinition(const RibbonControlDefinition &other)
          : BaseType(other)
      {
      }

      // Declaring this operator hides the generic node operator<< of the base,
      // so a RibbonControl accepts RibbonItems only.
      RibbonControlDefinition &operator<<(const RibbonItemDefinition &item)
      {
        BaseType::operator<<(Box().size(item.width_, 0)
                             << Button(ButtonProps().text(item.title_).onClick(item.onClick_)));
        return *this;
      }
    };
    typedef RibbonControlDefinition RibbonControl;
  } // namespace app
} // namespace loka

#endif // LOKA_APP_NODES_CONTROLS_RIBBON_HPP
