#ifndef LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "BmiCalculatorComponent.hpp"
#include "ToolboxControlSlots.hpp"
#include "MainNode.hpp"

namespace helloworld
{
  struct MainLeftPanelTypeTag
  {
  };

  class MainLeftPanelNode;

  struct MainLeftPanelProps : public declara::core::scene::NodePropsBase<MainLeftPanelProps>
  {
    typedef MainLeftPanelTypeTag TypeTag;
    typedef MainLeftPanelNode NodeType;
    MainNode *owner;
    MainLeftPanelProps() : owner(0) {}
    explicit MainLeftPanelProps(MainNode *o) : owner(o) {}
    bool operator<(const declara::core::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
        return false;
      const MainLeftPanelProps &other = static_cast<const MainLeftPanelProps &>(rhs);
      return owner < other.owner;
    }
  };

  class MainLeftPanelNode : public declara::core::scene::StaticCompositionBoundaryNodeBase<MainLeftPanelProps>
  {
  public:
    MainLeftPanelNode(const MainLeftPanelProps &p)
        : declara::core::scene::StaticCompositionBoundaryNodeBase<MainLeftPanelProps>(p) {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      if (!this->props.owner)
      {
        return;
      }
      c.declare(
          VStack()
          << Text("Loka Sample")
          << Text(this->props.owner->messageState())
          << Button("Add +", &this->props.owner->toggleEvent()).toolboxControl(kToolboxControlAddButton)
          << declara::core::scene::LightComponent(this->props.owner->bmiCalculator()));
    }
  };
} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
