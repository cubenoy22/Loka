#ifndef LOKA_MAC_EDIT_TEXT_CONTEXT_HPP
#define LOKA_MAC_EDIT_TEXT_CONTEXT_HPP

#include "MacRetirableContext.hpp"
#include "core/State.hpp"
#include "core/String.hpp"
#include "app/scene/state/WriteSeat.hpp"

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

class MacScenePlatformController;

class MacEditTextContext : public MacRetirableContext
{
public:
  /** Borrow a context only from this rail's class-checked native participant. */
  static MacEditTextContext *fromNativeFocus(void *responder);
  MacEditTextContext(MacScenePlatformController *controller,
                     void *parentView,
                     int x,
                     int y,
                     int width,
                     int height,
                     loka::app::EditTextNode *node);
  virtual ~MacEditTextContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  /** Attach-time read (late-subscriber rule): presentation from the current
      fact, called by the installing handler right after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);

  void handleTextDidChange();
  void *nativeField() const;
  void relayout(int x, int y, int width, int height);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  void bindText();
  void unbindText();
  void applyText();
  void syncStateFromControl();
  static void TextChangedThunk(void *userData);

  loka::app::EditTextNode *node_;
  void *field_;
  void *delegate_;
  loka::core::State<loka::core::String> *textState_;
  loka::app::scene::WriteSeat<loka::core::String> textSeat_;
  bool applyingFromState_;
  bool updatingFromControl_;
};

void RegisterMacEditTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_EDIT_TEXT_CONTEXT_HPP
