#include "MainNode.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "loka/dsl/Expr.hpp"
#include "loka/dsl/StateStream.hpp"

namespace helloworld
{
  static const loka::core::String kFruitItems[] =
      {
          loka::core::String::Literal("Apple"),
          loka::core::String::Literal("Banana"),
          loka::core::String::Literal("Cherry"),
          loka::core::String::Literal("Grape"),
      };
  static const std::size_t kFruitItemCount = sizeof(kFruitItems) / sizeof(kFruitItems[0]);

  MainRightPanelComponent::MainRightPanelComponent(MainNode *owner)
      : owner_(owner),
        initialized_(false),
        fruitIndex_(),
        fruitMessage_(),
        fruits_()
  {
    fruits_.assign(kFruitItems, kFruitItemCount);
  }

  void MainRightPanelComponent::attachNode(loka::app::scene::NodeComposition &c)
  {
    if (initialized_)
    {
      return;
    }
    c.declareStates()
        .state(fruitIndex_, 0)
        .state(fruitMessage_, loka::core::String::Literal("You chose Apple."));
    loka::dsl::StateStream<int> fruitIndexStream = fruitIndex_.stream();
    fruitIndexStream.map(loka::dsl::Const("You chose ")
                         + loka::dsl::At(fruits_, fruitIndexStream.slot.value())
                         + loka::dsl::Const("."))
        .set(fruitMessage_);
    initialized_ = true;
  }

  void MainRightPanelComponent::composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    c.declare(
        VStack().TEST_ID("HelloWorld.RightPanel")
        << Text("Fruit Picker").TEST_ID("HelloWorld.RightPanel.Title")
        << PopupMenu(fruits_.map<loka::core::String>(FruitPopupLabel()))
               .selectedIndex(fruitIndex_.state())
               .TEST_ID("HelloWorld.RightPanel.FruitPopup")
        << Text(fruitMessage_.state()).TEST_ID("HelloWorld.RightPanel.FruitMessage"));
  }
} // namespace helloworld
