#ifndef LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP

#include "BmiCalculatorComponent.hpp"

#include "app/nodes/controls/PopupMenu.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "core/State.hpp"
#include "core/String.hpp"
#include "core/Vector.hpp"

namespace helloworld
{
  inline loka::app::VStack MainRightPanel(const loka::Vector<loka::core::String> *fruits,
                                          const loka::app::scene::NodeState<int> &fruitIndex,
                                          loka::core::State<loka::core::String> *fruitMessage,
                                          const loka::app::scene::NodeState<loka::core::String> &heightInput,
                                          const loka::app::scene::NodeState<loka::core::String> &weightInput,
                                          loka::core::State<loka::core::String> *bmiResult)
  {
    using namespace loka::app;
    return VStack().TEST_ID("HelloWorld.RightPanel")
           << Text("Fruit Picker").TEST_ID("HelloWorld.RightPanel.Title")
           << PopupMenu(fruits) //
                  .selectedIndex(fruitIndex)
                  .TEST_ID("HelloWorld.RightPanel.FruitPopup")
           << Text(fruitMessage).TEST_ID("HelloWorld.RightPanel.FruitMessage")
           << BmiCalculator(heightInput, weightInput, bmiResult);
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
