#include "MainNode.hpp"

#include "BmiCalculatorComponent.hpp"
#include "app/nodes/Text.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "platform/StringUTF8.hpp"

#include <cstdio>
#include <cstdlib>

namespace helloworld
{
  using namespace loka::core;
  namespace
  {
    static const String kFruitItems[] = {
        String::Literal("Apple"),
        String::Literal("Banana"),
        String::Literal("Cherry"),
        String::Literal("Grape"),
    };
    static const std::size_t kFruitItemCount = sizeof(kFruitItems) / sizeof(kFruitItems[0]);
  } // namespace

  MainNode::MainNode(const MainProps &p)
      : loka::app::scene::StdCompositionNodeFor<MainNode>(MainProps(p)),
        initialized_(false),
        actionSummaryCacheValid_(false),
        lastActionSummaryEnabled_(false),
        lastActionSummaryCount_(0),
        bmiCacheValid_(false),
        lastBmiWasValid_(false),
        lastBmiHundredths_(0),
        message_(),
        toggleEvent_(),
        actionEnabled_(),
        actionProbeCount_(),
        actionSummary_(),
        heightInput_(),
        weightInput_(),
        bmiResult_(),
        toggleActionEnabledEvent_(),
        actionProbeEvent_(),
        fruitIndex_(),
        fruitMessage_(),
        fruits_()
  {
    this->state(this->message_, String::Literal("Hello, Loka!"));
    this->state(this->actionEnabled_, true);
    this->state(this->actionProbeCount_, 0);
    this->state(this->actionSummary_, String::Literal("Button enabled: yes / clicks: 0"));
    this->state(this->heightInput_, String::Literal("170.0"));
    this->state(this->weightInput_, String::Literal("60.0"));
    this->state(this->bmiResult_, String::Literal("BMI: --"));
    this->state(this->fruitIndex_, 0);
    this->state(this->fruitMessage_, String::Literal("You chose Apple."));
    this->fruits_.assign(kFruitItems, kFruitItemCount);
  }

  void MainNode::attachNode(loka::app::scene::NodeComposition &c)
  {
    (void)c;
    if (this->initialized_)
    {
      return;
    }
    this->bindActionForUi(this->toggleEvent_, &MainNode::toggleMessage);
    this->bindActionForUi(this->toggleActionEnabledEvent_, &MainNode::toggleActionEnabled);
    this->bindActionForUi(this->actionProbeEvent_, &MainNode::handleActionProbe);
    this->watchStateForUi(this->heightInput_, &MainNode::refreshBmiResult);
    this->watchStateForUi(this->weightInput_, &MainNode::refreshBmiResult);
    this->watchStateForUi(this->fruitIndex_, &MainNode::refreshFruitMessage);
    this->refreshActionSummary();
    this->refreshBmiResult();
    this->refreshFruitMessage();
    this->initialized_ = true;
  }

  ::Window *MainNode::windowOrNull() const
  {
    const AttachedContext *ctx = this->attachedContext();
    return ctx ? ctx->window() : 0;
  }

  loka::app::VStack MainNode::mainLeftPanel()
  {
    using namespace loka::app;
    return VStack().TEST_ID("HelloWorld.LeftPanel")
           << Text("Loka Sample").TEST_ID("HelloWorld.LeftPanel.Title")
           << Text(this->message_.state()).TEST_ID("HelloWorld.LeftPanel.Message")
           << Button("Add +", &this->toggleEvent_).TEST_ID("HelloWorld.LeftPanel.AddButton")
           << Text(this->actionSummary_.state()).TEST_ID("HelloWorld.LeftPanel.ActionSummary")
           << Button("Probe Button", &this->actionProbeEvent_)
                  .enabled(this->actionEnabled_.state())
                  .TEST_ID("HelloWorld.LeftPanel.ProbeButton")
           << Button("Toggle Button Enabled", &this->toggleActionEnabledEvent_)
                  .TEST_ID("HelloWorld.LeftPanel.ToggleEnabledButton")
           << BmiCalculator(this->heightInput_.state(), this->weightInput_.state(), this->bmiResult_.state());
  }

  double MainNode::parseBmiValue(const String &value) const
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return 0.0;
    }
    const char *start = utf8.c_str();
    char *endPtr = 0;
    const double parsed = std::strtod(start, &endPtr);
    if (endPtr == start)
    {
      return 0.0;
    }
    return parsed;
  }

  void MainNode::refreshBmiResult()
  {
    if (!this->heightInput_.isValid() || !this->weightInput_.isValid() || !this->bmiResult_.isValid())
    {
      return;
    }
    const double heightCm = this->parseBmiValue(this->heightInput_.get());
    const double weightKg = this->parseBmiValue(this->weightInput_.get());
    if (heightCm <= 0.0 || weightKg <= 0.0)
    {
      if (this->bmiCacheValid_ && !this->lastBmiWasValid_)
      {
        return;
      }
      this->bmiCacheValid_ = true;
      this->lastBmiWasValid_ = false;
      this->bmiResult_.set(String::Literal("BMI: --"));
      return;
    }
    const double heightM = heightCm / 100.0;
    if (heightM <= 0.0)
    {
      if (this->bmiCacheValid_ && !this->lastBmiWasValid_)
      {
        return;
      }
      this->bmiCacheValid_ = true;
      this->lastBmiWasValid_ = false;
      this->bmiResult_.set(String::Literal("BMI: --"));
      return;
    }
    const double bmi = weightKg / (heightM * heightM);
    if (bmi <= 0.0)
    {
      if (this->bmiCacheValid_ && !this->lastBmiWasValid_)
      {
        return;
      }
      this->bmiCacheValid_ = true;
      this->lastBmiWasValid_ = false;
      this->bmiResult_.set(String::Literal("BMI: --"));
      return;
    }
    const int bmiHundredths = static_cast<int>(bmi * 100.0 + 0.5);
    if (this->bmiCacheValid_ && this->lastBmiWasValid_ && this->lastBmiHundredths_ == bmiHundredths)
    {
      return;
    }
    this->bmiCacheValid_ = true;
    this->lastBmiWasValid_ = true;
    this->lastBmiHundredths_ = bmiHundredths;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "BMI: %.2f", bmi);
    this->bmiResult_.set(String(std::string(buf)));
  }

  void MainNode::toggleMessage()
  {
    if (!this->message_.isValid())
    {
      return;
    }
    ::Window *window = this->windowOrNull();
    this->message_.set(this->message_.get() + " +Loka");
    if (!window)
    {
      return;
    }
    {
      StateTrackerGuard _(window->getTracker());
      const String title = window->titleState().get();
      if (title.equals(String::Literal("LokaSample")))
      {
        window->titleState().set(String::Literal("LokaSample*"));
      }
      else
      {
        window->titleState().set(String::Literal("LokaSample"));
      }
    }
  }

  void MainNode::toggleActionEnabled()
  {
    if (!this->actionEnabled_.isValid())
    {
      return;
    }
    this->actionEnabled_.set(!this->actionEnabled_.get());
    this->refreshActionSummary();
  }

  void MainNode::handleActionProbe()
  {
    if (!this->actionProbeCount_.isValid())
    {
      return;
    }
    this->actionProbeCount_.set(this->actionProbeCount_.get() + 1);
    this->refreshActionSummary();
  }

  void MainNode::refreshActionSummary()
  {
    if (!this->actionEnabled_.isValid() || !this->actionProbeCount_.isValid() || !this->actionSummary_.isValid())
    {
      return;
    }
    const bool enabled = this->actionEnabled_.get();
    const int count = this->actionProbeCount_.get();
    if (this->actionSummaryCacheValid_ && this->lastActionSummaryEnabled_ == enabled
        && this->lastActionSummaryCount_ == count)
    {
      return;
    }
    this->actionSummaryCacheValid_ = true;
    this->lastActionSummaryEnabled_ = enabled;
    this->lastActionSummaryCount_ = count;
    const String enabledText = enabled ? String::Literal("yes") : String::Literal("no");
    const String countText = String::FromInt(count);
    this->actionSummary_.set(String::Literal("Button enabled: ") + enabledText + String::Literal(" / clicks: ")
                             + countText);
  }

  void MainNode::refreshFruitMessage()
  {
    if (!this->fruitIndex_.isValid() || !this->fruitMessage_.isValid())
    {
      return;
    }
    if (this->fruits_.empty())
    {
      this->fruitMessage_.set(String::Literal("You chose ."));
      return;
    }
    int index = this->fruitIndex_.get();
    if (index < 0 || static_cast<std::size_t>(index) >= this->fruits_.size())
    {
      index = 0;
    }
    this->fruitMessage_.set(String::Literal("You chose ") + this->fruits_[static_cast<std::size_t>(index)]
                            + String::Literal("."));
  }

  void MainNode::composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    ZStack &root = c.declare(ZStack().TEST_ID("HelloWorld.Root"));
    loka::app::scene::NodeComposition::ParentScope scope(c, root);
    c.declare(HStack().TEST_ID("HelloWorld.MainPanels")
              << this->mainLeftPanel()
              << MainRightPanel(&this->fruits_, this->fruitIndex_.state(), this->fruitMessage_.state()));
    c.declare(Text("*").TEST_ID("HelloWorld.Decoration"));
  }
} // namespace helloworld
