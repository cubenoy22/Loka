#ifndef LOKA_NULL_TEXT_EDITOR_CONTEXT_HPP
#define LOKA_NULL_TEXT_EDITOR_CONTEXT_HPP
#include "app/nodes/controls/TextEditor.hpp"
#include "app/scene/state/RequestSettlement.hpp"
#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
class NullScenePlatformController;
namespace loka
{
  namespace testing
  {
    class TextEditorInput;
  }
} // namespace loka
/** Fake native control. Bytes are tentative only inside INPUT; reconciliation
    completes after its outermost input unwinds. No undo/scroll state is modeled. */
class NullTextEditorContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit NullTextEditorContext(loka::app::TextEditorNode *node);
  void readLifecycleFactOnAttach();
  void syncFromNode();
  virtual void onPropsApplied()
  {
    this->syncFromNode();
  }
  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state);
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact, loka::app::scene::NodeLifecycleFact next);

private:
  friend class loka::testing::TextEditorInput;
  enum Phase
  {
    IDLE,
    INPUT,
    RECONCILE
  };
  loka::app::EditorResult input(const std::string &bytes, bool join, const loka::app::LineCursor *move);
  class RailOperation;
  void settle(loka::app::scene::Settlement stimulus
#ifdef TEST_BUILD
              , loka::app::LineCursor before
#endif
              );
  void restoreCommittedProjection(loka::app::LineCursor snapshot);
  void project(loka::app::LineCursor fallback);
  std::size_t nativeOffset() const;
  loka::app::TextEditorNode *node_;
  std::string buffer_;
  loka::app::LineCursor caret_;
  Phase phase_;
  loka::app::EditorResult status_;
  unsigned restores_;
};
namespace loka
{
  namespace testing
  {
    /** Test-only native event and observation doors; no stored callbacks. */
    class TextEditorInput
    {
    public:
      static app::EditorResult type(NullTextEditorContext &c, char value)
      {
        return c.input(std::string(1, value), false, 0);
      }
      static app::EditorResult enter(NullTextEditorContext &c)
      {
        return c.input("\r", false, 0);
      }
      static app::EditorResult backspace(NullTextEditorContext &c)
      {
        return c.input("", true, 0);
      }
      static app::EditorResult paste(NullTextEditorContext &c, const std::string &text)
      {
        return c.input(text, false, 0);
      }
      static app::EditorResult move(NullTextEditorContext &c, app::LineCursor cursor)
      {
        return c.input("", false, &cursor);
      }
      static const std::string &buffer(const NullTextEditorContext &c)
      {
        return c.buffer_;
      }
      static app::LineCursor caret(const NullTextEditorContext &c)
      {
        return c.caret_;
      }
      static unsigned restores(const NullTextEditorContext &c)
      {
        return c.restores_;
      }
      static app::EditorResult status(const NullTextEditorContext &c)
      {
        return c.status_;
      }
    };
  } // namespace testing
} // namespace loka
void RegisterNullTextEditorNodeHandler(NullScenePlatformController &controller);
const void *NullTextEditorNodeHandlerKey();
bool IsNullTextEditorNodeHandler(const loka::app::scene::IPlatformNodeHandler *handler);
#endif
