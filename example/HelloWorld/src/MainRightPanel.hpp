#ifndef LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP

#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include "app/RowColumn.hpp"
#include "app/PopupMenu.hpp"

namespace helloworld
{
  struct MainRightPanelProps
  {
    struct FruitPopupLabel
    {
      typedef loka::core::String Result;
      loka::core::String operator()(const loka::core::String &value) const { return value; }
    };

    MainRightPanelProps()
        : fruits(0),
          fruitIndex(0),
          fruitMessage(0) {}

    MainRightPanelProps(const loka::Vector<loka::core::String> *fruitValues,
                        loka::core::State<int> *fruitIndexValue,
                        loka::core::State<loka::core::String> *fruitMessageValue)
        : fruits(fruitValues),
          fruitIndex(fruitIndexValue),
          fruitMessage(fruitMessageValue) {}

    const loka::Vector<loka::core::String> *fruits;
    loka::core::State<int> *fruitIndex;
    loka::core::State<loka::core::String> *fruitMessage;
  };

  loka::app::VStack MainRightPanel(const MainRightPanelProps &props);

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
