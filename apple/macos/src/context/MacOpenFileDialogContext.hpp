#ifndef LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "app/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  }
}

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
  loka::core::MutableState<bool> *visibleState_;
  loka::core::MutableState<loka::app::FileChooserResult> *resultState_;
  loka::core::EmitterState *onResult_;
  bool presenting_;
};

void RegisterMacOpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
