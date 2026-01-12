#ifndef LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP

#include "core2/scene/Component.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "MainNode.hpp"

namespace helloworld
{
  class MainRightPanelComponent
  {
  public:
    explicit MainRightPanelComponent(MainNode *owner) : owner_(owner) {}

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
          << Text("Fruit Picker")
          << PopupMenu(owner_->fruits().map<loka::core::String>(MainNode::FruitPopupLabel()))
                 .selectedIndex(owner_->fruitIndexState())
                 .onChange(&owner_->fruitChangedEvent())
          << Text(owner_->fruitMessageState());
      parent << column;
    }

  private:
    MainNode *owner_;
  };
} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
