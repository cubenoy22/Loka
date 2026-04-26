#ifndef LOKA_HELLOWORLD_MAIN_NODE_HPP
#define LOKA_HELLOWORLD_MAIN_NODE_HPP

#include "MainLeftPanel.hpp"
#include "MainRightPanel.hpp"
#include "app/scene/NodeState.hpp"
#include "app/scene/nodes/boundary/StdComposition.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"

namespace helloworld {
  class MainNode;
  typedef loka::app::scene::StdCompositionPropsFor<MainNode> MainProps;

  class MainNode : public loka::app::scene::StdCompositionNodeFor<MainNode> {
  public:
    MainNode(const MainProps &p);
    virtual void attachNode(loka::app::scene::NodeComposition &c);
    virtual void composeNode(loka::app::scene::NodeComposition &c);

  private:
    ::Window *windowOrNull() const;
    double parseBmiValue(const loka::core::String &value) const;
    void refreshBmiResult();
    void toggleMessage();
    void toggleActionEnabled();
    void handleActionProbe();
    void refreshActionSummary();
    void refreshFruitMessage();

    bool initialized_;
    bool actionSummaryCacheValid_;
    bool lastActionSummaryEnabled_;
    int lastActionSummaryCount_;
    bool bmiCacheValid_;
    bool lastBmiWasValid_;
    int lastBmiHundredths_;
    loka::app::scene::NodeState<loka::core::String> message_;
    loka::core::EmitterState toggleEvent_;
    loka::app::scene::NodeState<bool> actionEnabled_;
    loka::app::scene::NodeState<int> actionProbeCount_;
    loka::app::scene::NodeState<loka::core::String> actionSummary_;
    loka::app::scene::NodeState<loka::core::String> heightInput_;
    loka::app::scene::NodeState<loka::core::String> weightInput_;
    loka::app::scene::NodeState<loka::core::String> bmiResult_;
    loka::core::EmitterState toggleActionEnabledEvent_;
    loka::core::EmitterState actionProbeEvent_;
    loka::app::scene::NodeState<int> fruitIndex_;
    loka::app::scene::NodeState<loka::core::String> fruitMessage_;
    loka::Vector<loka::core::String> fruits_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_NODE_HPP
