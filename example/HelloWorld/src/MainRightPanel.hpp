#ifndef LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP

#include "core/State.hpp"
#include "core2/scene/BoundState.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"

namespace declara
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
    void attachNode(declara::core::scene::NodeComposition &c);
    void composeNode(declara::core::scene::NodeComposition &c);

  private:
    void handleFruitChanged();

    MainNode *owner_;
    bool initialized_;
    declara::core::scene::BoundState<int> fruitIndex_;
    declara::core::scene::BoundState<loka::core::String> fruitMessage_;
    loka::Vector<loka::core::String> fruits_;
    declara::core::EmitterState fruitChangedEvent_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_RIGHT_PANEL_HPP
