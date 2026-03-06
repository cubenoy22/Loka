#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "app/AppComposition.hpp"
#include "app/AppConfigurable.hpp"
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"
#include "app/WindowDefinition.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "app/Menu.hpp"
#include "app/OpenFileDialog.hpp"
#include "core/resource/Image.hpp"
#include "loka/dsl/dsl.hpp"
#include "SimpleViewerFlowAdapters.hpp"
#include "MainNode.hpp"
#include <string>

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx),
        openDialogVisible_(false),
        chooserResult_(),
        chooserMessage_(loka::core::String::Literal("(none)")),
        image_(),
        flow_(buildFlow(this)),
        tracker_(),
        openDialogEvent_()
  {
    this->tracker_.addState(&this->openDialogVisible_);
    this->tracker_.addState(&this->chooserResult_);
    this->tracker_.addState(&this->chooserMessage_);
    this->tracker_.addState(&this->image_);
    this->openDialogEvent_.deferBind(&MyAppConfig::Invoke<&MyAppConfig::runOpenDialogPipeline>, this);
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(40, 40, 320, 240)
                       .scene(loka::app::scene::NodeDefinition<simpleviewer::MainProps, simpleviewer::MainNode>(
                           simpleviewer::MainProps()
                               .dialogVisible(&this->openDialogVisible_)
                               .message(&this->chooserMessage_)
                               .result(&this->chooserResult_)
                               .image(&this->image_)))
                       .title("LokaSimpleViewer")
                       .visible(true));
  }

  virtual void composeMenu(loka::app::MenuComposition &c)
  {
    using namespace loka::app;
    c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
                        << MenuSeparator()
                        << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
    c.declare(Menu("File") << MenuItem("Open...").onClick(&this->openDialogEvent_));
  }

private:
  typedef loka::dsl::FlowChain<loka::app::FileChooserResult, loka::core::resource::Image> ViewerFlowChain;

  enum FlowStepId
  {
    FLOW_STEP_CHOOSER_TO_CONTEXT = 1,
    FLOW_STEP_CONTEXT_TO_PROJECTION = 2,
    FLOW_STEP_PROJECTION_TO_BLOB = 3,
    FLOW_STEP_BLOB_DECODE_ATTEMPT = 4,
    FLOW_STEP_DECODE_ATTEMPT_TO_IMAGE = 5
  };

  void runOpenDialogPipeline()
  {
    this->applyOpenDialogVisible();
  }

  void applyOpenDialogVisible()
  {
    loka::core::StateTrackerGuard guard(&this->tracker_);
    if (this->openDialogVisible_.get())
    {
      this->openDialogVisible_.set(false, true);
    }
    this->openDialogVisible_.set(true, true);
  }

  static loka::dsl::FlowHandleResult OnBlobDecodeFailure(const loka::dsl::FlowError &error, void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->image_.set(loka::core::resource::Image::Empty(), true);
      self->chooserMessage_.set(loka::core::String::Literal("Decode error ")
                                    + loka::core::String::FromInt(error.code),
                                true);
    }
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  static ViewerFlowChain buildFlow(MyAppConfig *self)
  {
    ViewerFlowChain chain =
        loka::dsl::Flow()
        | loka::dsl::Step(FLOW_STEP_CHOOSER_TO_CONTEXT, simpleviewer::ChooserToContextAdapter())
        | loka::dsl::Step(FLOW_STEP_CONTEXT_TO_PROJECTION, simpleviewer::ContextToProjectionAdapter())
              .onSuccess(&self->chooserMessage_, &simpleviewer::ChooserProjection::message)
        | loka::dsl::Step(FLOW_STEP_PROJECTION_TO_BLOB, simpleviewer::ProjectionToBlobAdapter())
        | loka::dsl::Step(FLOW_STEP_BLOB_DECODE_ATTEMPT,
                          simpleviewer::BlobToDecodeAttemptAdapter(self->getPlatformContext()))
              .onFailure(&MyAppConfig::OnBlobDecodeFailure, self)
        | loka::dsl::Step(FLOW_STEP_DECODE_ATTEMPT_TO_IMAGE, simpleviewer::DecodeAttemptToImageAdapter())
              .onSuccess(&self->image_);
    chain.bindTrigger(&self->chooserResult_);
    chain.withTracker(&self->tracker_);
    return chain;
  }

  template <void (MyAppConfig::*Method)()>
  static void Invoke(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      (self->*Method)();
    }
  }

  loka::core::MutableState<bool> openDialogVisible_;
  loka::core::MutableState<loka::app::FileChooserResult> chooserResult_;
  loka::core::MutableState<loka::core::String> chooserMessage_;
  loka::core::MutableState<loka::core::resource::Image> image_;
  ViewerFlowChain flow_;
  loka::core::PushStateTracker tracker_;
  loka::core::EmitterState openDialogEvent_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
