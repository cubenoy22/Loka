#ifndef LOKA_MAC_TEXT_EDITOR_CONTEXT_HPP
#define LOKA_MAC_TEXT_EDITOR_CONTEXT_HPP

#include "MacRetirableContext.hpp"
#include "app/nodes/controls/TextEditor.hpp"

namespace loka
{
  namespace testing
  {
    class MacTextEditorAccess;
  }
} // namespace loka
namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka

/** Whole-document native projection. The node owns the mutation seam; the
    context owns native presentation and cancellable reconciliation only. */
class MacTextEditorContext : public MacRetirableContext
{
public:
  MacTextEditorContext(MacScenePlatformController *, void *parent, loka::app::TextEditorNode *);
  virtual ~MacTextEditorContext();
  bool hasNativeView() const;
  void readLifecycleFactOnAttach();
  virtual void onPropsApplied();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact, loka::app::scene::NodeLifecycleFact);
  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &);
  /** Native notification origin; each caller supplies its authoritative caret. */
  enum TextObservation
  {
    VIEW_CHANGE,
    STORAGE_EDIT
  };
  void handleTextDidChange(TextObservation source, std::size_t caretOffset);
  void handleSelectionDidChange();
  void captureSelection();
  void restoreCommittedProjection();

private:
  friend class loka::testing::MacTextEditorAccess;
  struct Projection;
  Projection *projection_;
  unsigned restores_;
  loka::app::TextEditorNode *node_;
  void *parent_;
  void *scroll_;
  void *delegate_;
  void syncFromNode(bool force);
  void scheduleRestore();
  loka::app::EditorResult applyNativeChange(std::size_t caretOffset);
};

void RegisterMacTextEditorNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &);
#endif
