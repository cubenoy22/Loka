#ifndef DECLARA_HELLOWORLD_BMI_FORM_COMPONENT_HPP
#define DECLARA_HELLOWORLD_BMI_FORM_COMPONENT_HPP

#include <string>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "core/State.hpp"
#include "core/Window.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "core2/scene/node/Group.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "BmiCalculatorComponent.hpp"

namespace helloworld
{
  class BmiFormNode;
  typedef declara::core::scene::GroupPropsFor<BmiFormNode> BmiFormProps;

  class BmiFormNode : public declara::core::scene::GroupNodeBase<BmiFormProps>
  {
  public:
    BmiFormProps props;
    BmiFormNode(const BmiFormProps &p)
        : declara::core::scene::GroupNodeBase<BmiFormProps>(BmiFormProps(p)),
          props(p),
          heightInput_(0),
          weightInput_(0),
          bmiResult_(0),
          message_(0),
          boundary_(0),
          window_(0),
          toggleEvent_()
    {
      // State is initialized lazily in composeNode via NodeComposition::useState.
    }

    virtual void prepareNode(declara::core::scene::NodeComposition &c)
    {
      heightInput_ = &c.useState<std::string>("170.0");
      weightInput_ = &c.useState<std::string>("60.0");
      bmiResult_ = &c.useState<std::string>("BMI: --");
      message_ = &c.useState<std::string>("Hello, Loka!");
      boundary_ = c.boundary();
      window_ = c.window();
      assert(boundary_ && "BmiFormNode requires a BoundaryNode");
      assert(window_ && "BmiFormNode requires a Window");
      heightInput_->bind(&BmiFormNode::InputChangedThunk, this, false);
      weightInput_->bind(&BmiFormNode::InputChangedThunk, this, false);
      toggleEvent_.bind(&BmiFormNode::ToggleMessageThunk, this, false);
      updateBmi();
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(
          VStack()
          << Text("Loka Sample")
          << Text(message_)
          << Button("Toggle Message", &toggleEvent_)
          << BmiCalculator(c, BmiCalculatorProps(heightInput_, weightInput_, bmiResult_)));
    }

  private:
    static double parseDouble(const std::string &value)
    {
      const char *start = value.c_str();
      char *endPtr = 0;
      double parsed = std::strtod(start, &endPtr);
      if (endPtr == start)
      {
        return 0.0;
      }
      return parsed;
    }

    static void InputChangedThunk(void *userData)
    {
      BmiFormNode *self = static_cast<BmiFormNode *>(userData);
      if (self)
      {
        self->updateBmi();
      }
    }

    static void ToggleMessageThunk(void *userData)
    {
      BmiFormNode *self = static_cast<BmiFormNode *>(userData);
      if (self)
      {
        self->toggleMessage();
      }
    }

    void toggleMessage()
    {
      if (!message_)
      {
        return;
      }
      std::string next = "Hello, Loka!";
      if (message_->get() == "Hello, Loka!")
      {
        next = "Loka says hi!";
      }
      {
        StateTrackerGuard _(boundary_->tracker());
        message_->set(next, true);
      }

      assert(boundary_ && "BmiFormNode toggleMessage requires a BoundaryNode");
      assert(window_ && "BmiFormNode toggleMessage requires a Window");
      {
        StateTrackerGuard _(window_->getTracker());
        const std::string title = window_->titleState().get();

        if (title == "LokaSample")
        {
          window_->titleState().set("LokaSample*");
        }
        else
        {
          window_->titleState().set("LokaSample");
        }
      }
    }

    void updateBmi()
    {
      double heightCm = parseDouble(heightInput_->get());
      double weightKg = parseDouble(weightInput_->get());
      std::string label("BMI: --");
      if (heightCm > 0.0 && weightKg > 0.0)
      {
        double heightM = heightCm / 100.0;
        if (heightM > 0.0)
        {
          double bmi = weightKg / (heightM * heightM);
          char buf[64];
          std::sprintf(buf, "BMI: %.2f", bmi);
          label = buf;
        }
      }
      StateTrackerGuard _(boundary_->tracker());
      bmiResult_->set(label, true);
    }

    MutableState<std::string> *heightInput_;
    MutableState<std::string> *weightInput_;
    MutableState<std::string> *bmiResult_;
    MutableState<std::string> *message_;
    declara::core::scene::BoundaryNode *boundary_;
    Window *window_;
    EmitterState toggleEvent_;
  };

  inline declara::core::scene::NodeDefinition<BmiFormProps, BmiFormNode> BmiForm()
  {
    return declara::core::scene::NodeDefinition<BmiFormProps, BmiFormNode>();
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_BMI_FORM_COMPONENT_HPP
