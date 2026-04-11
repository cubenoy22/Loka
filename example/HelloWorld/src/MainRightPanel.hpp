#ifndef LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP

#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"

namespace helloworld
{
  struct FruitPopupLabel
  {
    typedef loka::core::String Result;
    loka::core::String operator()(const loka::core::String &value) const { return value; }
  };

  inline loka::app::VStack MainRightPanel(const loka::Vector<loka::core::String> *fruits,
                                          loka::core::State<int> *fruitIndex,
                                          loka::core::State<loka::core::String> *fruitMessage)
  {
    using namespace loka::app;
    return VStack().TEST_ID("HelloWorld.RightPanel")
           << Text("Fruit Picker").TEST_ID("HelloWorld.RightPanel.Title")
           << PopupMenu(fruits->map<loka::core::String>(FruitPopupLabel()))
                  .selectedIndex(fruitIndex)
                  .TEST_ID("HelloWorld.RightPanel.FruitPopup")
           << Text(fruitMessage).TEST_ID("HelloWorld.RightPanel.FruitMessage");
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
