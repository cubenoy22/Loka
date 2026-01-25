#ifndef LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP

#include "loka/core/State.hpp"
#include "core2/scene/BoundState.hpp"
#include "loka/core/String.hpp"
#include "BmiCalculatorComponent.hpp"

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

  class MainLeftPanelComponent
  {
  public:
    explicit MainLeftPanelComponent(MainNode *owner);
    void attachNode(loka::app::scene::NodeComposition &c);
    void composeNode(loka::app::scene::NodeComposition &c);

  private:
    void toggleMessage();

    MainNode *owner_;
    bool initialized_;
    loka::app::scene::BoundState<loka::core::String> message_;
    loka::core::EmitterState toggleEvent_;
    BmiCalculatorComponent bmiCalculator_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
