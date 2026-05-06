#ifndef LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"

class ToolboxOpenFileDialogContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit ToolboxOpenFileDialogContext(loka::app::OpenFileDialogNode *node);
  virtual ~ToolboxOpenFileDialogContext();
  virtual void onNodeAttached();
  virtual void onNodeDetached();
  void presentIfNeeded();

private:
  struct NativeDialogSession;

  void presentDialog();
  void setResult(const loka::app::FileChooserResult &result);
  void disposeDialog();
  NativeDialogSession *detachDialogIfActive(NativeDialogSession *dialog);

  loka::app::OpenFileDialogNode *node_;
  loka::core::MutableState<loka::app::FileChooserResult> *resultState_;
  loka::core::EmitterState *onResult_;
  loka::core::MutableState<bool> *closeState_;
  loka::app::OpenFileDialogPresentationPhase presentation_;
  NativeDialogSession *dialog_;
};

#endif // LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
