#ifndef DECLARA_MAC_EDIT_TEXT_CONTEXT_HPP
#define DECLARA_MAC_EDIT_TEXT_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "core/State.hpp"

namespace declara
{
  namespace app
  {
    class EditTextNode;
  }
}

class MacEditTextContext : public declara::core::scene::NativeNodeContext
{
public:
  MacEditTextContext(void *parentView, int x, int y, int width, int height, declara::app::EditTextNode *node);
  virtual ~MacEditTextContext();

  void handleTextDidChange();

private:
  void bindText();
  void unbindText();
  void applyText();
  void syncStateFromControl();
  static void TextChangedThunk(void *userData);

  declara::app::EditTextNode *node_;
  void *field_;
  void *delegate_;
  State<std::string> *textState_;
  bool applyingFromState_;
  bool updatingFromControl_;
};

#endif // DECLARA_MAC_EDIT_TEXT_CONTEXT_HPP
