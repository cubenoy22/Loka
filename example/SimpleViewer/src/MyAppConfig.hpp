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
#include "core/resource/Blob.hpp"
#include "core/resource/BlobLoader.hpp"
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
        blobRequest_(),
        blob_(),
        image_(),
        tracker_(),
        openDialogEvent_()
  {
    this->tracker_.addState(&this->openDialogVisible_);
    this->tracker_.addState(&this->chooserResult_);
    this->tracker_.addState(&this->chooserMessage_);
    this->tracker_.addState(&this->blobRequest_);
    this->tracker_.addState(&this->blob_);
    this->tracker_.addState(&this->image_);
    this->openDialogEvent_.deferBind(&MyAppConfig::OpenDialogThunk, this);
    this->chooserResult_.deferBind(&MyAppConfig::ChooserResultThunk, this);
    this->blob_.deferBind(&MyAppConfig::BlobChangedThunk, this);
    this->blobLoader_.attach(&this->blobRequest_, &this->blob_, 0);
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
  enum FlowStepId
  {
    FLOW_STEP_CHOOSER_PROJECTION = 1,
    FLOW_STEP_BLOB_TO_IMAGE = 2
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

  static void OpenDialogThunk(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->runOpenDialogPipeline();
    }
  }

  void runChooserPipeline()
  {
    const loka::app::FileChooserResult result = this->chooserResult_.get();
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(FLOW_STEP_CHOOSER_PROJECTION, simpleviewer::ChooserToProjectionAdapter())
              .input(&result)
              .onSuccess(&projection);
    (void)chain.run();

    this->applyChooserProjection(projection);
  }

  void runBlobPipeline()
  {
    PlatformContext *ctx = this->getPlatformContext();
    if (!ctx)
    {
      return;
    }

    const loka::core::resource::Blob blob = this->blob_.get();
    loka::core::resource::Image image;

    loka::dsl::FlowChain<loka::core::resource::Blob, loka::core::resource::Image> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(FLOW_STEP_BLOB_TO_IMAGE, simpleviewer::BlobToImageAdapter(ctx))
              .input(&blob)
              .onSuccess(&image);
    (void)chain.run();

    this->applyDecodedImage(image);
  }

  void applyChooserProjection(const simpleviewer::ChooserProjection &projection)
  {
    loka::core::StateTrackerGuard guard(&this->tracker_);
    this->chooserMessage_.set(projection.message, true);
    this->blobRequest_.set(projection.request, true);
  }

  void applyDecodedImage(const loka::core::resource::Image &image)
  {
    loka::core::StateTrackerGuard guard(&this->tracker_);
    this->image_.set(image, true);
  }

  static void ChooserResultThunk(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->runChooserPipeline();
    }
  }

  static void BlobChangedThunk(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->runBlobPipeline();
    }
  }

  loka::core::MutableState<bool> openDialogVisible_;
  loka::core::MutableState<loka::app::FileChooserResult> chooserResult_;
  loka::core::MutableState<loka::core::String> chooserMessage_;
  loka::core::MutableState<loka::core::resource::BlobLoaderRequest> blobRequest_;
  loka::core::MutableState<loka::core::resource::Blob> blob_;
  loka::core::MutableState<loka::core::resource::Image> image_;
  loka::core::resource::BlobLoader blobLoader_;
  loka::core::PushStateTracker tracker_;
  loka::core::EmitterState openDialogEvent_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
