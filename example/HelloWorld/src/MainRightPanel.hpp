#ifndef LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "MainNode.hpp"

namespace helloworld
{
  struct MainRightPanelTypeTag
  {
  };

  class MainRightPanelNode;

  struct MainRightPanelProps : public declara::core::scene::NodePropsBase<MainRightPanelProps>
  {
    typedef MainRightPanelTypeTag TypeTag;
    typedef MainRightPanelNode NodeType;
    MainNode *owner;
    MainRightPanelProps() : owner(0) {}
    explicit MainRightPanelProps(MainNode *o) : owner(o) {}
    bool operator<(const declara::core::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
        return false;
      const MainRightPanelProps &other = static_cast<const MainRightPanelProps &>(rhs);
      return owner < other.owner;
    }
  };

  class MainRightPanelNode : public declara::core::scene::StaticCompositionBoundaryNodeBase<MainRightPanelProps>
  {
  public:
    MainRightPanelNode(const MainRightPanelProps &p)
        : declara::core::scene::StaticCompositionBoundaryNodeBase<MainRightPanelProps>(p) {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      if (!this->props.owner)
      {
        return;
      }
      c.declare(
          VStack()
          << Text("Fruit Picker")
          << PopupMenu(this->props.owner->fruits().map<loka::core::String>(MainNode::FruitPopupLabel()))
                 .selectedIndex(this->props.owner->fruitIndexState())
                 .onChange(&this->props.owner->fruitChangedEvent())
          << Text(this->props.owner->fruitMessageState()));
    }
  };
} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
