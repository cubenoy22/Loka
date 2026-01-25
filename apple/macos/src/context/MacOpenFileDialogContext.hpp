#ifndef LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"
#include "loka/core/String.hpp"

class MacOpenFileDialogContext : public loka::app::scene::NativeNodeContext
{
public:
  MacOpenFileDialogContext(void *parentView, loka::app::OpenFileDialogNode *node);
  virtual ~MacOpenFileDialogContext();

private:
  void bindVisible();
  void unbindVisible();
  void applyVisible();
  void presentDialog();
  void setResult(const loka::app::FileChooserResult &result);

  static void VisibleChangedThunk(void *userData);

  void *parentView_;
  loka::app::OpenFileDialogNode *node_;
  loka::core::State<bool> *visibleState_;
  loka::core::MutableState<loka::app::FileChooserResult> *resultState_;
  loka::core::EmitterState *onResult_;
  bool lastVisible_;
  bool presenting_;
};

#endif // LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
