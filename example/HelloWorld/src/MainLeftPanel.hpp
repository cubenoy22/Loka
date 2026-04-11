#ifndef LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
#define LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP

#include "loka/core/State.hpp"
#include "app/scene/BoundState.hpp"
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
    static void BmiChangedThunk(void *userData);
    double parseBmiValue(const loka::core::String &value) const;
    void refreshBmiResult();
    void toggleMessage();
    void toggleActionEnabled();
    void handleActionProbe();
    void refreshActionSummary();

    MainNode *owner_;
    bool initialized_;
    bool actionSummaryCacheValid_;
    bool lastActionSummaryEnabled_;
    int lastActionSummaryCount_;
    loka::app::scene::BoundState<loka::core::String> message_;
    loka::core::EmitterState toggleEvent_;
    loka::app::scene::BoundState<bool> actionEnabled_;
    loka::app::scene::BoundState<int> actionProbeCount_;
    loka::app::scene::BoundState<loka::core::String> actionSummary_;
    bool bmiCacheValid_;
    bool lastBmiWasValid_;
    int lastBmiHundredths_;
    loka::app::scene::BoundState<loka::core::String> heightInput_;
    loka::app::scene::BoundState<loka::core::String> weightInput_;
    loka::app::scene::BoundState<loka::core::String> bmiResult_;
    loka::core::EmitterState toggleActionEnabledEvent_;
    loka::core::EmitterState actionProbeEvent_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_LEFT_PANEL_HPP
