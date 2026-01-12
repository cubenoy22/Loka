#ifndef LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP

#include "core2/scene/Component.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "BmiCalculatorComponent.hpp"
#include "ToolboxControlSlots.hpp"
#include "MainNode.hpp"

namespace helloworld
{
  class MainLeftPanelComponent
  {
  public:
    explicit MainLeftPanelComponent(MainNode *owner) : owner_(owner) {}

    void attachNode(declara::core::scene::NodeComposition &) {}

    void composeNode(declara::core::scene::NodeComposition &, declara::core::scene::INestableDefinition &parent)
    {
      using namespace declara::app;
      if (!owner_)
      {
        return;
      }
      VStack column;
      column
          << Text("Loka Sample")
          << Text(owner_->messageState())
          << Button("Add +", &owner_->toggleEvent()).toolboxControl(kToolboxControlAddButton)
          << declara::core::scene::LightComponent(owner_->bmiCalculator());
      parent << column;
    }

  private:
    MainNode *owner_;
  };
} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
