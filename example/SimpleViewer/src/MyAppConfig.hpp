#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "core/AppComposition.hpp"
#include "core/AppConfigurable.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/WindowDefinition.hpp"
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
                       .scene(declara::core::scene::NodeDefinition<simpleviewer::MainProps, simpleviewer::MainNode>(
                           simpleviewer::MainProps()
                               .dialogVisible(&this->openDialogVisible_)
                               .message(&this->chooserMessage_)
                               .result(&this->chooserResult_)
                               .onResult(&this->chooserResultEvent_)))
                       .title("LokaSimpleViewer")
                       .visible(true));
  }

  virtual void composeMenu(declara::app::MenuComposition &c)
  {
    using namespace declara::app;
    c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
                        << MenuSeparator()
                        << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
    c.declare(Menu("File") << MenuItem("Open...").onClick(&this->openDialogEvent_));
  }

private:
  void handleOpenDialog()
  {
    StateTrackerGuard guard(&this->tracker_);
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
    const declara::app::FileChooserResult result = this->chooserResult_.get();
    const loka::core::String message = formatChooserMessage(result);
    StateTrackerGuard guard(&this->tracker_);
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

  static loka::core::String formatChooserMessage(const declara::app::FileChooserResult &result)
  {
    using namespace declara::app;
    switch (result.kind)
    {
    case FileChooserResult::RESULT_FILE:
      return loka::core::String::Literal("File: ") + formatFileRef(result.file);
    case FileChooserResult::RESULT_FOLDER:
      return loka::core::String::Literal("Folder: ") + formatFolderRef(result.folder);
    case FileChooserResult::RESULT_CANCELED:
      return loka::core::String::Literal("Canceled");
    case FileChooserResult::RESULT_ERROR:
      return loka::core::String::Literal("Error ") + loka::core::String::FromInt(result.errorCode);
    default:
      return loka::core::String::Literal("(none)");
    }
  }

  static loka::core::String formatFileRef(const declara::app::FileRef &ref)
  {
#if defined(LOKA_RETRO68)
    if (ref.kind == declara::app::FileRef::KIND_FSSPEC)
    {
      char name[64];
      const unsigned char length = ref.spec.name[0];
      const unsigned char capped = length > 63 ? 63 : length;
      for (unsigned char i = 0; i < capped; ++i)
      {
        name[i] = static_cast<char>(ref.spec.name[i + 1]);
      }
      name[capped] = '\0';
      return loka::core::String(std::string(name, capped));
    }
#endif
    if (ref.kind == declara::app::FileRef::KIND_PATH && !ref.path.empty())
    {
      return ref.path;
    }
    return loka::core::String::Literal("(unknown)");
  }

  static loka::core::String formatFolderRef(const declara::app::FolderRef &ref)
  {
#if defined(LOKA_RETRO68)
    if (ref.kind == declara::app::FolderRef::KIND_FSSPEC)
    {
      char name[64];
      const unsigned char length = ref.spec.name[0];
      const unsigned char capped = length > 63 ? 63 : length;
      for (unsigned char i = 0; i < capped; ++i)
      {
        name[i] = static_cast<char>(ref.spec.name[i + 1]);
      }
      name[capped] = '\0';
      return loka::core::String(std::string(name, capped));
    }
#endif
    if (ref.kind == declara::app::FolderRef::KIND_PATH && !ref.path.empty())
    {
      return ref.path;
    }
    return loka::core::String::Literal("(unknown)");
  }

  declara::core::MutableState<bool> openDialogVisible_;
  declara::core::MutableState<declara::app::FileChooserResult> chooserResult_;
  declara::core::MutableState<loka::core::String> chooserMessage_;
  declara::core::PushStateTracker tracker_;
  declara::core::EmitterState openDialogEvent_;
  declara::core::EmitterState chooserResultEvent_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
