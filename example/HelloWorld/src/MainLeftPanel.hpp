#ifndef LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP

#include "app/Button.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/Text.hpp"
#include "loka/core/String.hpp"
#include "loka/core/State.hpp"
#include "BmiCalculatorComponent.hpp"

namespace helloworld
{
  inline loka::app::VStack MainLeftPanel(loka::core::State<loka::core::String> *message,
                                         loka::core::EmitterState *toggleEvent,
                                         loka::core::State<loka::core::String> *actionSummary,
                                         loka::core::EmitterState *actionProbeEvent,
                                         loka::core::State<bool> *actionEnabled,
                                         loka::core::EmitterState *toggleActionEnabledEvent,
                                         loka::core::State<loka::core::String> *heightInput,
                                         loka::core::State<loka::core::String> *weightInput,
                                         loka::core::State<loka::core::String> *bmiResult)
  {
    using namespace loka::app;
    return VStack().TEST_ID("HelloWorld.LeftPanel")
           << Text("Loka Sample").TEST_ID("HelloWorld.LeftPanel.Title")
           << Text(message).TEST_ID("HelloWorld.LeftPanel.Message")
           << Button("Add +", toggleEvent).TEST_ID("HelloWorld.LeftPanel.AddButton")
           << Text(actionSummary).TEST_ID("HelloWorld.LeftPanel.ActionSummary")
           << Button("Probe Button", actionProbeEvent).enabled(actionEnabled).TEST_ID("HelloWorld.LeftPanel.ProbeButton")
           << Button("Toggle Button Enabled", toggleActionEnabledEvent).TEST_ID("HelloWorld.LeftPanel.ToggleEnabledButton")
           << BmiCalculator(heightInput, weightInput, bmiResult);
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
