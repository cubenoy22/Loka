#include "MainRightPanel.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"

namespace helloworld
{
  loka::app::VStack MainRightPanel(const MainRightPanelProps &props)
  {
    using namespace loka::app;
    return VStack().TEST_ID("HelloWorld.RightPanel")
           << Text("Fruit Picker").TEST_ID("HelloWorld.RightPanel.Title")
           << PopupMenu(props.fruits->map<loka::core::String>(MainRightPanelProps::FruitPopupLabel()))
                  .selectedIndex(props.fruitIndex)
                  .TEST_ID("HelloWorld.RightPanel.FruitPopup")
           << Text(props.fruitMessage).TEST_ID("HelloWorld.RightPanel.FruitMessage");
  }
} // namespace helloworld
