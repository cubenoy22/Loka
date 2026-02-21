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
        openDialogEvent_(),
        chooserResultEvent_()
  {
    this->tracker_.addState(&this->openDialogVisible_);
    this->tracker_.addState(&this->chooserResult_);
    this->tracker_.addState(&this->chooserMessage_);
    this->tracker_.addState(&this->blobRequest_);
    this->tracker_.addState(&this->blob_);
    this->tracker_.addState(&this->image_);
    this->openDialogEvent_.deferBind(&MyAppConfig::OpenDialogThunk, this);
    this->chooserResultEvent_.deferBind(&MyAppConfig::ChooserResultThunk, this);
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
                               .onResult(&this->chooserResultEvent_)
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
  void handleOpenDialog()
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
      self->handleOpenDialog();
    }
  }

  void handleChooserResult()
  {
    const loka::app::FileChooserResult result = this->chooserResult_.get();
    const loka::core::String message = formatChooserMessage(result);
    loka::core::StateTrackerGuard guard(&this->tracker_);
    this->chooserMessage_.set(message, true);
    this->updateBlobRequest(result);
  }

  static void ChooserResultThunk(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->handleChooserResult();
    }
  }

  void updateBlobRequest(const loka::app::FileChooserResult &result)
  {
    using namespace loka::core::resource;
    BlobLoaderRequest request;
    if (result.kind == loka::app::FileChooserResult::RESULT_FILE)
    {
      const loka::core::String path = result.item.toString();
      if (!path.empty())
      {
        request.setFilePath(path);
        request.setTag(loka::core::String::Literal("image-file"));
      }
    }
    this->blobRequest_.set(request, true);
  }

  void handleBlobChanged()
  {
    PlatformContext *ctx = this->getPlatformContext();
    if (!ctx)
    {
      return;
    }
    const loka::core::resource::Blob blob = this->blob_.get();
    loka::core::resource::Image image;
    if (ctx->createImageFromBlob(blob, image))
    {
      loka::core::StateTrackerGuard guard(&this->tracker_);
      this->image_.set(image, true);
      return;
    }
    loka::core::StateTrackerGuard guard(&this->tracker_);
    this->image_.set(loka::core::resource::Image::Empty(), true);
  }

  static void BlobChangedThunk(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->handleBlobChanged();
    }
  }

  static loka::core::String formatChooserMessage(const loka::app::FileChooserResult &result)
  {
    using namespace loka::app;
    switch (result.kind)
    {
    case FileChooserResult::RESULT_FILE:
      return loka::core::String::Literal("Loka file: ") + formatItem(result.item);
    case FileChooserResult::RESULT_FOLDER:
      return loka::core::String::Literal("Loka folder: ") + formatItem(result.item);
    case FileChooserResult::RESULT_CANCELED:
      return loka::core::String::Literal("Canceled");
    case FileChooserResult::RESULT_ERROR:
      return loka::core::String::Literal("Error ") + loka::core::String::FromInt(result.errorCode);
    default:
      return loka::core::String::Literal("(none)");
    }
  }

  static loka::core::String formatItem(const loka::file::File &item)
  {
    const loka::core::String path = item.toString();
    return path.empty() ? loka::core::String::Literal("(unknown)") : path;
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
  loka::core::EmitterState chooserResultEvent_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
