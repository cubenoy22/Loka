#ifndef LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP
#define LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP

#include <windows.h>
#include "app/scene/projection/NativeNodeContext.hpp"
#include "core/String.hpp"

namespace loka
{
  namespace core
  {
    template <typename T> class State;
    template <typename T> class MutableState;
  } // namespace core
} // namespace loka

namespace loka
{
  namespace app
  {
    class EditTextNode;
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka

class Win32EditTextContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32EditTextContext(HWND parent, int x, int y, int width, int height, loka::app::EditTextNode *node);
  virtual ~Win32EditTextContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  /** Attach-time read (late-subscriber rule): presentation from the current
      fact, called by the installing handler right after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);

  HWND hwnd() const
  {
    return hwnd_;
  }
  void relayout(int x, int y, int width, int height);
  bool handleCommand(WPARAM wParam, LPARAM lParam);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  void bindText();
  void unbindText();
  void applyText();
  void syncStateFromControl();
  static void TextChangedThunk(void *userData);

  loka::app::EditTextNode *node_;
  HWND hwnd_;
  loka::core::State<loka::core::String> *textState_;
  bool applyingFromState_;
  bool updatingFromControl_;
};

void RegisterWin32EditTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP
