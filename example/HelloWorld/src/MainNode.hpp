#ifndef LOKA_HELLOWORLD_MAIN_NODE_HPP
#define LOKA_HELLOWORLD_MAIN_NODE_HPP

#include "app/scene/node/StaticComposition.hpp"
#include "MainLeftPanel.hpp"
#include "MainRightPanel.hpp"
#include "app/ZStack.hpp"
#include "app/Text.hpp"
#include "app/scene/BoundState.hpp"
#include "app/Window.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "loka/dsl/Expr.hpp"
#include "loka/dsl/StateStream.hpp"
#include <cstdio>
#include <cstdlib>

class Window;

namespace helloworld
{
  class MainNode;
  typedef loka::app::scene::StaticCompositionPropsFor<MainNode> MainProps;

  class MainNode : public loka::app::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : loka::app::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
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
      static const loka::core::String kFruitItems[] =
          {
              loka::core::String::Literal("Apple"),
              loka::core::String::Literal("Banana"),
              loka::core::String::Literal("Cherry"),
              loka::core::String::Literal("Grape"),
          };
      const std::size_t fruitItemCount = sizeof(kFruitItems) / sizeof(kFruitItems[0]);
      this->fruits_.assign(kFruitItems, fruitItemCount);
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates()
          .state(this->message_, loka::core::String::Literal("Hello, Loka!"))
          .state(this->actionEnabled_, true)
          .state(this->actionProbeCount_, 0)
          .state(this->actionSummary_, loka::core::String::Literal("Button enabled: yes / clicks: 0"))
          .state(this->heightInput_, loka::core::String::Literal("170.0"))
          .state(this->weightInput_, loka::core::String::Literal("60.0"))
          .state(this->bmiResult_, loka::core::String::Literal("BMI: --"))
          .state(this->fruitIndex_, 0)
          .state(this->fruitMessage_, loka::core::String::Literal("You chose Apple."));
      this->bindForUi(this->toggleEvent_, this, &MainNode::toggleMessage);
      this->bindForUi(this->toggleActionEnabledEvent_, this, &MainNode::toggleActionEnabled);
      this->bindForUi(this->actionProbeEvent_, this, &MainNode::handleActionProbe);
      this->heightInput_.bind(&MainNode::BmiChangedThunk, this, false);
      this->weightInput_.bind(&MainNode::BmiChangedThunk, this, false);
      {
        loka::dsl::StateStream<int> fruitIndexStream = this->fruitIndex_.stream();
        fruitIndexStream.map(loka::dsl::Const("You chose ")
                             + loka::dsl::At(this->fruits_, fruitIndexStream.slot.value())
                             + loka::dsl::Const("."))
            .set(this->fruitMessage_);
      }
      this->refreshActionSummary();
      this->refreshBmiResult();
      this->initialized_ = true;
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *ctx = this->attachedContext();
      return ctx ? ctx->window() : 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c);

  private:
    static void BmiChangedThunk(void *userData)
    {
      MainNode *self = static_cast<MainNode *>(userData);
      if (self)
      {
        self->refreshBmiResult();
      }
    }

    double parseBmiValue(const loka::core::String &value) const
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

    void refreshBmiResult()
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
        this->bmiResult_.set(loka::core::String::Literal("BMI: --"));
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
        this->bmiResult_.set(loka::core::String::Literal("BMI: --"));
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
        this->bmiResult_.set(loka::core::String::Literal("BMI: --"));
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
      this->bmiResult_.set(loka::core::String(std::string(buf)));
    }

    void toggleMessage()
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
        loka::core::StateTrackerGuard _(window->getTracker());
        const loka::core::String title = window->titleState().get();
        if (title.equals(loka::core::String::Literal("LokaSample")))
        {
          window->titleState().set(loka::core::String::Literal("LokaSample*"));
        }
        else
        {
          window->titleState().set(loka::core::String::Literal("LokaSample"));
        }
      }
    }

    void toggleActionEnabled()
    {
      if (!this->actionEnabled_.isValid())
      {
        return;
      }
      this->actionEnabled_.set(!this->actionEnabled_.get());
      this->refreshActionSummary();
    }

    void handleActionProbe()
    {
      if (!this->actionProbeCount_.isValid())
      {
        return;
      }
      this->actionProbeCount_.set(this->actionProbeCount_.get() + 1);
      this->refreshActionSummary();
    }

    void refreshActionSummary()
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
      const loka::core::String enabledText = enabled ? loka::core::String::Literal("yes") : loka::core::String::Literal("no");
      const loka::core::String countText = loka::core::String::FromInt(count);
      this->actionSummary_.set(loka::core::String::Literal("Button enabled: ")
                               + enabledText
                               + loka::core::String::Literal(" / clicks: ")
                               + countText);
    }

    bool initialized_;
    bool actionSummaryCacheValid_;
    bool lastActionSummaryEnabled_;
    int lastActionSummaryCount_;
    bool bmiCacheValid_;
    bool lastBmiWasValid_;
    int lastBmiHundredths_;
    loka::app::scene::BoundState<loka::core::String> message_;
    loka::core::EmitterState toggleEvent_;
    loka::app::scene::BoundState<bool> actionEnabled_;
    loka::app::scene::BoundState<int> actionProbeCount_;
    loka::app::scene::BoundState<loka::core::String> actionSummary_;
    loka::app::scene::BoundState<loka::core::String> heightInput_;
    loka::app::scene::BoundState<loka::core::String> weightInput_;
    loka::app::scene::BoundState<loka::core::String> bmiResult_;
    loka::core::EmitterState toggleActionEnabledEvent_;
    loka::core::EmitterState actionProbeEvent_;
    loka::app::scene::BoundState<int> fruitIndex_;
    loka::app::scene::BoundState<loka::core::String> fruitMessage_;
    loka::Vector<loka::core::String> fruits_;
  };

  inline void MainNode::composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    ZStack &root = c.declare(ZStack().TEST_ID("HelloWorld.Root"));
    loka::app::scene::NodeComposition::ParentScope scope(c, root);
    c.declare(HStack().TEST_ID("HelloWorld.MainPanels")
              << MainLeftPanel(MainLeftPanelProps(this->message_.state(),
                                                  &this->toggleEvent_,
                                                  this->actionSummary_.state(),
                                                  &this->actionProbeEvent_,
                                                  this->actionEnabled_.state(),
                                                  &this->toggleActionEnabledEvent_,
                                                  this->heightInput_.state(),
                                                  this->weightInput_.state(),
                                                  this->bmiResult_.state()))
              << MainRightPanel(MainRightPanelProps(&this->fruits_,
                                                    this->fruitIndex_.state(),
                                                    this->fruitMessage_.state())));
    c.declare(Text("*").TEST_ID("HelloWorld.Decoration"));
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_NODE_HPP
