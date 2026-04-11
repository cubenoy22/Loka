#include "MainNode.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "app/Window.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "loka/platform/StringUTF8.hpp"
#include <cstdio>
#include <cstdlib>

namespace helloworld {
  MainLeftPanelComponent::MainLeftPanelComponent(MainNode *owner)
      : owner_(owner), initialized_(false), message_(), toggleEvent_(), actionEnabled_(), actionProbeCount_(),
        actionSummary_(), bmiCacheValid_(false), lastBmiWasValid_(false), lastBmiHundredths_(0), heightInput_(),
        weightInput_(), bmiResult_(), toggleActionEnabledEvent_(), actionProbeEvent_() {
    this->actionSummaryCacheValid_ = false;
    this->lastActionSummaryEnabled_ = false;
    this->lastActionSummaryCount_ = 0;
  }

  void MainLeftPanelComponent::attachNode(loka::app::scene::NodeComposition &c) {
    if (initialized_) {
      return;
    }
    c.declareStates()
        .state(message_, loka::core::String::Literal("Hello, Loka!"))
        .state(actionEnabled_, true)
        .state(actionProbeCount_, 0)
        .state(actionSummary_, loka::core::String::Literal("Button enabled: yes / clicks: 0"))
        .state(heightInput_, loka::core::String::Literal("170.0"))
        .state(weightInput_, loka::core::String::Literal("60.0"))
        .state(bmiResult_, loka::core::String::Literal("BMI: --"));
    if (owner_) {
      owner_->bindForUiComponent(toggleEvent_, this, &MainLeftPanelComponent::toggleMessage);
      owner_->bindForUiComponent(toggleActionEnabledEvent_, this, &MainLeftPanelComponent::toggleActionEnabled);
      owner_->bindForUiComponent(actionProbeEvent_, this, &MainLeftPanelComponent::handleActionProbe);
    }
    heightInput_.bind(&MainLeftPanelComponent::BmiChangedThunk, this, false);
    weightInput_.bind(&MainLeftPanelComponent::BmiChangedThunk, this, false);
    refreshActionSummary();
    refreshBmiResult();
    initialized_ = true;
  }

  void MainLeftPanelComponent::composeNode(loka::app::scene::NodeComposition &c) {
    using namespace loka::app;
    if (!owner_) {
      return;
    }
    c.declare(VStack().TEST_ID("HelloWorld.LeftPanel")
                       << Text("Loka Sample").TEST_ID("HelloWorld.LeftPanel.Title")
                       << Text(message_.state()).TEST_ID("HelloWorld.LeftPanel.Message")
                       << Button("Add +", &toggleEvent_).TEST_ID("HelloWorld.LeftPanel.AddButton")
                       << Text(actionSummary_.state()).TEST_ID("HelloWorld.LeftPanel.ActionSummary")
                       << Button("Probe Button", &actionProbeEvent_).enabled(actionEnabled_.state()).TEST_ID("HelloWorld.LeftPanel.ProbeButton")
                       << Button("Toggle Button Enabled", &toggleActionEnabledEvent_).TEST_ID("HelloWorld.LeftPanel.ToggleEnabledButton")
                       << BmiCalculator(BmiCalculatorProps(heightInput_.state(), weightInput_.state(), bmiResult_.state())));
  }

  void MainLeftPanelComponent::BmiChangedThunk(void *userData) {
    MainLeftPanelComponent *self = static_cast<MainLeftPanelComponent *>(userData);
    if (self) {
      self->refreshBmiResult();
    }
  }

  double MainLeftPanelComponent::parseBmiValue(const loka::core::String &value) const {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8)) {
      return 0.0;
    }
    const char *start = utf8.c_str();
    char *endPtr = 0;
    const double parsed = std::strtod(start, &endPtr);
    if (endPtr == start) {
      return 0.0;
    }
    return parsed;
  }

  void MainLeftPanelComponent::refreshBmiResult() {
    if (!heightInput_.isValid() || !weightInput_.isValid() || !bmiResult_.isValid()) {
      return;
    }
    const double heightCm = this->parseBmiValue(heightInput_.get());
    const double weightKg = this->parseBmiValue(weightInput_.get());
    if (heightCm <= 0.0 || weightKg <= 0.0) {
      if (this->bmiCacheValid_ && !this->lastBmiWasValid_) {
        return;
      }
      this->bmiCacheValid_ = true;
      this->lastBmiWasValid_ = false;
      this->bmiResult_.set(loka::core::String::Literal("BMI: --"));
      return;
    }
    const double heightM = heightCm / 100.0;
    if (heightM <= 0.0) {
      if (this->bmiCacheValid_ && !this->lastBmiWasValid_) {
        return;
      }
      this->bmiCacheValid_ = true;
      this->lastBmiWasValid_ = false;
      this->bmiResult_.set(loka::core::String::Literal("BMI: --"));
      return;
    }
    const double bmi = weightKg / (heightM * heightM);
    if (bmi <= 0.0) {
      if (this->bmiCacheValid_ && !this->lastBmiWasValid_) {
        return;
      }
      this->bmiCacheValid_ = true;
      this->lastBmiWasValid_ = false;
      this->bmiResult_.set(loka::core::String::Literal("BMI: --"));
      return;
    }
    const int bmiHundredths = static_cast<int>(bmi * 100.0 + 0.5);
    if (this->bmiCacheValid_ && this->lastBmiWasValid_ && this->lastBmiHundredths_ == bmiHundredths) {
      return;
    }
    this->bmiCacheValid_ = true;
    this->lastBmiWasValid_ = true;
    this->lastBmiHundredths_ = bmiHundredths;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "BMI: %.2f", bmi);
    this->bmiResult_.set(loka::core::String(std::string(buf)));
  }

  void MainLeftPanelComponent::toggleMessage() {
    if (!message_.isValid()) {
      return;
    }
    if (!owner_) {
      return;
    }
    ::Window *window = owner_->windowOrNull();
    loka::core::String next = message_.get() + " +Loka";
    message_.set(next);

    if (!window) {
      return;
    }
    {
      loka::core::StateTrackerGuard _(window->getTracker());
      const loka::core::String title = window->titleState().get();

      if (title.equals(loka::core::String::Literal("LokaSample"))) {
        window->titleState().set(loka::core::String::Literal("LokaSample*"));
      } else {
        window->titleState().set(loka::core::String::Literal("LokaSample"));
      }
    }
  }

  void MainLeftPanelComponent::toggleActionEnabled() {
    if (!actionEnabled_.isValid()) {
      return;
    }
    actionEnabled_.set(!actionEnabled_.get());
    refreshActionSummary();
  }

  void MainLeftPanelComponent::handleActionProbe() {
    if (!actionProbeCount_.isValid()) {
      return;
    }
    actionProbeCount_.set(actionProbeCount_.get() + 1);
    refreshActionSummary();
  }

  void MainLeftPanelComponent::refreshActionSummary() {
    if (!actionEnabled_.isValid() || !actionProbeCount_.isValid() || !actionSummary_.isValid()) {
      return;
    }
    const bool enabled = actionEnabled_.get();
    const int count = actionProbeCount_.get();
    if (this->actionSummaryCacheValid_ && this->lastActionSummaryEnabled_ == enabled
        && this->lastActionSummaryCount_ == count) {
      return;
    }
    this->actionSummaryCacheValid_ = true;
    this->lastActionSummaryEnabled_ = enabled;
    this->lastActionSummaryCount_ = count;
    const loka::core::String enabledText =
        enabled ? loka::core::String::Literal("yes") : loka::core::String::Literal("no");
    const loka::core::String countText = loka::core::String::FromInt(count);
    actionSummary_.set(loka::core::String::Literal("Button enabled: ") + enabledText
                           + loka::core::String::Literal(" / clicks: ") + countText);
  }
} // namespace helloworld
