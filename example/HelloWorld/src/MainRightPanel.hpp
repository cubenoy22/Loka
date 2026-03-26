#ifndef LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP

#include "loka/core/State.hpp"
#include "app/scene/BoundState.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"

namespace loka
{
  namespace core
  {
    namespace scene
    {
      struct NodeComposition;
    }
  }
}

namespace helloworld
{
  class MainNode;

  class MainRightPanelComponent
  {
  public:
    struct FruitPopupLabel
    {
      typedef loka::core::String Result;
      loka::core::String operator()(const loka::core::String &value) const { return value; }
    };

    explicit MainRightPanelComponent(MainNode *owner);
    void attachNode(loka::app::scene::NodeComposition &c);
    void composeNode(loka::app::scene::NodeComposition &c);

  private:
    void handleFruitChanged();

    MainNode *owner_;
    bool initialized_;
    int lastFruitMessageIndex_;
    loka::app::scene::BoundState<int> fruitIndex_;
    loka::app::scene::BoundState<loka::core::String> fruitMessage_;
    loka::Vector<loka::core::String> fruits_;
    loka::core::EmitterState fruitChangedEvent_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
