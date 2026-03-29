#ifndef LOKA_MAC_EDIT_TEXT_CONTEXT_HPP
#define LOKA_MAC_EDIT_TEXT_CONTEXT_HPP

#include "app/scene/NativeNodeContext.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace app
  {
    class EditTextNode;
  }
}

class MacEditTextContext : public loka::app::scene::NativeNodeContext
{
public:
  MacEditTextContext(void *parentView, int x, int y, int width, int height, loka::app::EditTextNode *node);
  virtual ~MacEditTextContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

  void handleTextDidChange();
  void *nativeField() const;
  void relayout(int x, int y, int width, int height);

private:
  void bindText();
  void unbindText();
  void applyText();
  void syncStateFromControl();
  static void TextChangedThunk(void *userData);

  loka::app::EditTextNode *node_;
  void *field_;
  void *delegate_;
  loka::core::State<loka::core::String> *textState_;
  bool applyingFromState_;
  bool updatingFromControl_;
};

#endif // LOKA_MAC_EDIT_TEXT_CONTEXT_HPP
