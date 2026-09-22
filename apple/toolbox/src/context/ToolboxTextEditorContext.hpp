#ifndef LOKA_TOOLBOX_TEXT_EDITOR_CONTEXT_HPP
#define LOKA_TOOLBOX_TEXT_EDITOR_CONTEXT_HPP
#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include <TextEdit.h>
#include "app/scene/projection/PlatformNodeHandler.hpp"
namespace loka
{
  namespace testing
  {
    class ToolboxTextEditorAccess;
  }
} // namespace loka
/** Plain whole-document projection. The controller's edit ledger owns TE;
    this context owns cancellation storage and the synchronous input phase. */
class ToolboxTextEditorContext : public ToolboxProjectedNodeContext
{
public:
  ToolboxTextEditorContext(loka::app::TextEditorNode *, ToolboxScenePlatformController *);
  virtual ~ToolboxTextEditorContext();
  virtual void onPropsApplied();
  virtual loka::app::scene::PaintAnswer queryPaintDamage(const loka::app::scene::PaintQuery &) const;
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact, loka::app::scene::NodeLifecycleFact);
  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &);
  virtual void render(loka::app::scene::IPlatformController *);
  void repaint(TEHandle);
  const Rect &chromeRect() const
  {
    return this->rect_;
  }
  loka::app::EditorResult key(char);
  loka::app::EditorResult click(const Point &);
  /** Bounded selection replacement door, also used by the scenario's paste action. */
  loka::app::EditorResult paste(const char *, std::size_t);
  void retryProjection();
  /** Called by every controller retirement path before queueing TE. */
  void invalidateNativePresentation();

private:
  friend class loka::testing::ToolboxTextEditorAccess;
  enum Phase
  {
    IDLE,
    INPUT,
    RECONCILE,
    PROJECT
  };
  bool hasStaleCaret() const;
  loka::app::EditorResult beginInput();
  enum Change
  {
    CARET_CHANGE,
    LINE_CHANGE,
    STRUCTURE_CHANGE
  };
  loka::app::EditorResult finishInput(loka::app::EditorResult, Change);
  void restoreCommittedProjection();
  void project();
  void consumePendingRequest();
  loka::app::LineCursor cursorAt(short) const;
  short offsetOf(loka::app::LineCursor) const;
  void updateRect(const Rect &);
  loka::app::TextEditorNode *node_;
  TEHandle te_;
  char *restore_;
  Rect rect_;
  Rect paintRect_;
  const loka::core::ObservableList<loka::core::String> *source_;
  loka::core::ListRevision revision_;
  Phase phase_;
  loka::app::EditorResult status_;
  unsigned restores_;
};
bool RegisterToolboxTextEditorNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &);
#endif
