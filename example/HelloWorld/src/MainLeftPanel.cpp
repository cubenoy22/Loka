#include "MainLeftPanel.hpp"
#include "app/Button.hpp"
#include "app/Text.hpp"

namespace helloworld {
  loka::app::VStack MainLeftPanel(const MainLeftPanelProps &props) {
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
