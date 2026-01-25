#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "app/AppComposition.hpp"
#include "app/AppConfigurable.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "app/WindowDefinition.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/Menu.hpp"
#include "app/OpenFileDialog.hpp"
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
        tracker_(),
        openDialogEvent_(),
        chooserResultEvent_()
  {
    this->tracker_.addState(&this->openDialogVisible_);
    this->tracker_.addState(&this->chooserResult_);
    this->tracker_.addState(&this->chooserMessage_);
    this->openDialogEvent_.deferBind(&MyAppConfig::OpenDialogThunk, this);
    this->chooserResultEvent_.deferBind(&MyAppConfig::ChooserResultThunk, this);
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(40, 40, 320, 240)
                       .scene(loka::core::scene::NodeDefinition<simpleviewer::MainProps, simpleviewer::MainNode>(
                           simpleviewer::MainProps()
                               .dialogVisible(&this->openDialogVisible_)
                               .message(&this->chooserMessage_)
                               .result(&this->chooserResult_)
                               .onResult(&this->chooserResultEvent_)))
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
  }

  static void ChooserResultThunk(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->handleChooserResult();
    }
  }

  static loka::core::String formatChooserMessage(const loka::app::FileChooserResult &result)
  {
    using namespace loka::app;
    switch (result.kind)
    {
    case FileChooserResult::RESULT_FILE:
      return loka::core::String::Literal("File: ") + formatItem(result.item);
    case FileChooserResult::RESULT_FOLDER:
      return loka::core::String::Literal("Folder: ") + formatItem(result.item);
    case FileChooserResult::RESULT_CANCELED:
      return loka::core::String::Literal("Canceled");
    case FileChooserResult::RESULT_ERROR:
      return loka::core::String::Literal("Error ") + loka::core::String::FromInt(result.errorCode);
    default:
      return loka::core::String::Literal("(none)");
    }
  }

  static loka::core::String formatItem(const loka::file::Item &item)
  {
    const loka::core::String path = item.toString();
    return path.empty() ? loka::core::String::Literal("(unknown)") : path;
  }

  loka::core::MutableState<bool> openDialogVisible_;
  loka::core::MutableState<loka::app::FileChooserResult> chooserResult_;
  loka::core::MutableState<loka::core::String> chooserMessage_;
  loka::core::PushStateTracker tracker_;
  loka::core::EmitterState openDialogEvent_;
  loka::core::EmitterState chooserResultEvent_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
