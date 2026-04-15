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
  virtual void onNodeAttached();
  virtual void onNodeDetached();
  void presentIfNeeded();
  void presentDeferred();

private:
  void presentDialog();
  void setResult(const loka::app::FileChooserResult &result);

  void *parentView_;
  loka::app::OpenFileDialogNode *node_;
  loka::core::MutableState<loka::app::FileChooserResult> *resultState_;
  loka::core::EmitterState *onResult_;
  loka::app::OpenFileDialogPresentationPhase presentation_;
  void *deferredPresenter_;
};

void RegisterMacOpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
