#include "MainNode.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"

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
        fruits_(),
        fruitChangedEvent_()
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
    if (owner_)
    {
      owner_->bindForUiComponent(fruitChangedEvent_, this, &MainRightPanelComponent::handleFruitChanged);
    }
    initialized_ = true;
  }

  void MainRightPanelComponent::composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    if (!owner_)
    {
      return;
    }
    c.declare(
        VStack().TEST_ID("HelloWorld.RightPanel")
        << Text("Fruit Picker").TEST_ID("HelloWorld.RightPanel.Title")
        << PopupMenu(fruits_.map<loka::core::String>(FruitPopupLabel()))
               .selectedIndex(fruitIndex_)
               .onChange(&fruitChangedEvent_)
               .TEST_ID("HelloWorld.RightPanel.FruitPopup")
        << Text(fruitMessage_).TEST_ID("HelloWorld.RightPanel.FruitMessage"));
  }

  void MainRightPanelComponent::handleFruitChanged()
  {
    if (!fruitIndex_.isValid() || !fruitMessage_.isValid() || fruits_.empty())
    {
      return;
    }
    int index = fruitIndex_.get();
    if (index < 0 || static_cast<std::size_t>(index) >= fruits_.size())
    {
      index = 0;
    }
    loka::core::String next = loka::core::String::Literal("You chose ") + fruits_[static_cast<std::size_t>(index)] + ".";
    fruitMessage_.set(next, true);
  }
} // namespace helloworld
