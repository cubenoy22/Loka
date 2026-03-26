#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "app/AppComposition.hpp"
#include "app/AppConfigurable.hpp"
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"
#include "app/WindowDefinition.hpp"
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
        isDialogShown_(false),
        chooserResult_(),
        chooserMessage_(loka::core::String::Literal("(none)")),
        image_(),
        flow_(buildFlow(this)),
        tracker_(),
        openDialogEvent_()
  {
    this->tracker_.addState(&this->isDialogShown_);
    this->tracker_.addState(&this->chooserResult_);
    this->tracker_.addState(&this->chooserMessage_);
    this->tracker_.addState(&this->image_);
    this->openDialogEvent_.deferBind(&MyAppConfig::OnOpenDialogEvent, this);
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(40, 40, 320, 240)
                       .scene(loka::app::scene::NodeDefinition<simpleviewer::MainProps, simpleviewer::MainNode>(
                           simpleviewer::MainProps()
                               .isDialogShown(&this->isDialogShown_)
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

  static void OnOpenDialogEvent(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (!self) return;
    self->isDialogShown_.set(true, true);
  }

  static loka::dsl::FlowHandleResult OnBlobDecodeFailure(const loka::dsl::FlowError &error, void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->setEmptyImageIfNeeded();
      self->setChooserMessageIfChanged(buildErrorMessage(error));
    }
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  static bool IsNoFileSelectedError(const loka::dsl::FlowError &error, void *)
  {
    return error.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED;
  }

  static loka::dsl::FlowHandleResult OnBlobLoadCanceled(const loka::dsl::FlowError &, void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (!self) return loka::dsl::FLOW_ERROR_HANDLED;
    self->setEmptyImageIfNeeded();
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  static loka::dsl::FlowHandleResult OnBlobLoadFailure(const loka::dsl::FlowError &error, void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (!self) return loka::dsl::FLOW_ERROR_HANDLED;
    self->setEmptyImageIfNeeded();
    self->setChooserMessageIfChanged(buildErrorMessage(error));
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  static loka::core::String buildErrorMessage(const loka::dsl::FlowError &error)
  {
    using namespace simpleviewer;
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_CONTEXT_MISSING)
    {
      return loka::core::String::Literal("Platform context is missing.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED)
    {
      return loka::core::String::Literal("Failed to read file.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_PATH_UTF8_CONVERT_FAILED)
    {
      return loka::core::String::Literal("Read failed: path UTF-8 conversion.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_OPENFILE_FAILED)
    {
      return loka::core::String::Literal("Read failed: platform openFile failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_NO_FSSPEC)
    {
      return loka::core::String::Literal("Read failed: Classic FSSpec missing.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_OPEN_DF_FAILED)
    {
      return loka::core::String::Literal("Read failed: FSpOpenDF failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_GETEOF_FAILED)
    {
      return loka::core::String::Literal("Read failed: GetEOF failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_READ_FAILED)
    {
      return loka::core::String::Literal("Read failed: FSRead failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED)
    {
      return loka::core::String::Literal("Read failed: fopen failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_SEEK_FAILED)
    {
      return loka::core::String::Literal("Read failed: fseek failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_READ_FAILED)
    {
      return loka::core::String::Literal("Read failed: fread failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED)
    {
      return loka::core::String::Literal("No file selected.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_DECODE_FAILED)
    {
      return loka::core::String::Literal("Failed to decode image. Classic supports mainly uncompressed PICT; QuickTime-compressed PICT or low memory may fail.");
    }
    return loka::core::String::Literal("Unexpected flow error code: ")
           + loka::core::String::FromInt(error.code);
  }

  static ViewerFlowChain buildFlow(MyAppConfig *self)
  {
    ViewerFlowChain chain =
        loka::dsl::Flow()
        | loka::dsl::Step(FLOW_STEP_CHOOSER_TO_CONTEXT, simpleviewer::ChooserToContextAdapter())
        | loka::dsl::Step(FLOW_STEP_CONTEXT_TO_PROJECTION, simpleviewer::ContextToProjectionAdapter())
              .onSuccess(&self->chooserMessage_, &simpleviewer::ChooserProjection::message)
        | loka::dsl::Step(FLOW_STEP_PROJECTION_TO_BLOB,
                          simpleviewer::ProjectionToBlobAdapter(self->getPlatformContext()))
              .onFailure(&MyAppConfig::IsNoFileSelectedError, &MyAppConfig::OnBlobLoadCanceled, self)
              .onFailure(&MyAppConfig::OnBlobLoadFailure, self)
        | loka::dsl::Step(FLOW_STEP_BLOB_DECODE_ATTEMPT,
                          simpleviewer::BlobToDecodeAttemptAdapter(self->getPlatformContext()))
              .onFailure(&MyAppConfig::OnBlobDecodeFailure, self)
        | loka::dsl::Step(FLOW_STEP_DECODE_ATTEMPT_TO_IMAGE, simpleviewer::DecodeAttemptToImageAdapter())
              .onSuccess(&self->image_);
    chain.bindTrigger(&self->chooserResult_);
    chain.withTracker(&self->tracker_);
    return chain;
  }

  void setEmptyImageIfNeeded()
  {
    const loka::core::resource::Image empty = loka::core::resource::Image::Empty();
    if (this->image_.get() == empty)
    {
      return;
    }
    this->image_.set(empty);
  }

  void setChooserMessageIfChanged(const loka::core::String &message)
  {
    if (this->chooserMessage_.get().equals(message))
    {
      return;
    }
    this->chooserMessage_.set(message);
  }

  loka::core::MutableState<bool> isDialogShown_;
  loka::core::MutableState<loka::app::FileChooserResult> chooserResult_;
  loka::core::MutableState<loka::core::String> chooserMessage_;
  loka::core::MutableState<loka::core::resource::Image> image_;
  ViewerFlowChain flow_;
  loka::core::PushStateTracker tracker_;
  loka::core::EmitterState openDialogEvent_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
