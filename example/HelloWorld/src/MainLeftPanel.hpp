#ifndef LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP

#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "loka/core/String.hpp"
#include "loka/core/State.hpp"
#include "BmiCalculatorComponent.hpp"

namespace helloworld
{
  struct MainLeftPanelProps
  {
    MainLeftPanelProps()
        : message(0),
          toggleEvent(0),
          actionSummary(0),
          actionProbeEvent(0),
          actionEnabled(0),
          toggleActionEnabledEvent(0),
          heightInput(0),
          weightInput(0),
          bmiResult(0) {}

    MainLeftPanelProps(loka::core::State<loka::core::String> *messageValue,
                       loka::core::EmitterState *toggleValue,
                       loka::core::State<loka::core::String> *actionSummaryValue,
                       loka::core::EmitterState *actionProbeValue,
                       loka::core::State<bool> *actionEnabledValue,
                       loka::core::EmitterState *toggleActionEnabledValue,
                       loka::core::State<loka::core::String> *heightValue,
                       loka::core::State<loka::core::String> *weightValue,
                       loka::core::State<loka::core::String> *bmiResultValue)
        : message(messageValue),
          toggleEvent(toggleValue),
          actionSummary(actionSummaryValue),
          actionProbeEvent(actionProbeValue),
          actionEnabled(actionEnabledValue),
          toggleActionEnabledEvent(toggleActionEnabledValue),
          heightInput(heightValue),
          weightInput(weightValue),
          bmiResult(bmiResultValue) {}

    loka::core::State<loka::core::String> *message;
    loka::core::EmitterState *toggleEvent;
    loka::core::State<loka::core::String> *actionSummary;
    loka::core::EmitterState *actionProbeEvent;
    loka::core::State<bool> *actionEnabled;
    loka::core::EmitterState *toggleActionEnabledEvent;
    loka::core::State<loka::core::String> *heightInput;
    loka::core::State<loka::core::String> *weightInput;
    loka::core::State<loka::core::String> *bmiResult;
  };

  inline loka::app::VStack MainLeftPanel(const MainLeftPanelProps &props)
  {
    using namespace loka::app;
    return VStack().TEST_ID("HelloWorld.LeftPanel")
           << Text("Loka Sample").TEST_ID("HelloWorld.LeftPanel.Title")
           << Text(props.message).TEST_ID("HelloWorld.LeftPanel.Message")
           << Button("Add +", props.toggleEvent).TEST_ID("HelloWorld.LeftPanel.AddButton")
           << Text(props.actionSummary).TEST_ID("HelloWorld.LeftPanel.ActionSummary")
           << Button("Probe Button", props.actionProbeEvent).enabled(props.actionEnabled).TEST_ID("HelloWorld.LeftPanel.ProbeButton")
           << Button("Toggle Button Enabled", props.toggleActionEnabledEvent).TEST_ID("HelloWorld.LeftPanel.ToggleEnabledButton")
           << BmiCalculator(BmiCalculatorProps(props.heightInput, props.weightInput, props.bmiResult));
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
