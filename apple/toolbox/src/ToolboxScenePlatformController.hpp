#ifndef LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP

#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/projection/PlatformLayoutHandler.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "debug/ToolboxSceneDebugStats.hpp"
#include <Quickdraw.h>
#include <Controls.h>
#include <TextEdit.h>

class ToolboxWindow;
class ToolboxButtonContext;
class ToolboxPopupMenuContext;
class ToolboxNodeContextMapper;
class ToolboxCellContext;

class ToolboxScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  explicit ToolboxScenePlatformController(ToolboxWindow *window);
  virtual ~ToolboxScenePlatformController();

  virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild);
  virtual void onBoundaryApply(loka::app::scene::Node *rootNode,
                               loka::app::scene::BoundaryNode *boundary,
                               const loka::app::scene::BoundaryLocalApplyInfo &info,
                               const loka::app::scene::PlatformApplyPlan &plan);
  virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const { return true; }
  virtual bool prepareProjectedLayout(loka::app::scene::Node *node, loka::app::scene::LayoutState &state);
  virtual void synchronize();
  virtual bool hasPendingSync() const;
  virtual void destroy();

  void render();
  void renderDirty(const Rect &rect);
  bool handleMouseDown(const Point &point);
  void emitHitEmitter(loka::core::EmitterState *emitter);
  void recordButtonHit(const Rect &rect,
                       loka::core::EmitterState *emitter,
                       loka::core::State<bool> *enabled,
                       loka::app::scene::BoundaryNode *boundary,
                       ToolboxButtonContext *context);
  void recordCellHit(const Rect &rect,
                     loka::core::EmitterState *emitter,
                     loka::app::scene::BoundaryNode *boundary,
                     ToolboxCellContext *context,
                     loka::core::State<loka::core::String> *text);
  void recordEditHit(const Rect &rect,
                     loka::core::State<loka::core::String> *text,
                     loka::app::scene::BoundaryNode *boundary);
  void recordTextHit(const Rect &rect,
                     short x,
                     short y,
                     loka::core::State<loka::core::String> *text,
                     loka::app::scene::BoundaryNode *boundary,
                     bool needsRelayoutOnChange,
                     short visibleWidth);
  void recordPopupHit(const Rect &rect,
                      short lineHeight,
                      const loka::Vector<loka::core::String> *items,
                      loka::core::State<int> *selectedIndex,
                      loka::core::EmitterState *onChange,
                      loka::core::State<bool> *enabled,
                      loka::app::scene::BoundaryNode *boundary,
                      short menuId,
                      ToolboxPopupMenuContext *context);
  void applyPopupSelectionChange(const Rect &rect,
                                 loka::app::scene::BoundaryNode *boundary,
                                 loka::core::State<int> *selectedIndex,
                                 loka::core::EmitterState *onChange,
                                 int newIndex);
  bool handleKeyDown(char key);
  bool handleControlClick(const Point &point);
  void drawControlsInRect(const Rect &rect);
  bool ensureButtonControl(short resourceId,
                           const Rect &rect,
                           const loka::core::String &label,
                           loka::core::EmitterState *emitter,
                           loka::core::State<bool> *enabled);
  void drawFallbackControl(const Rect &rect);
  TEHandle ensureEditTextControl(const Rect &rect, loka::core::State<loka::core::String> *text);
  void idleTextEdits();
  bool isPointInEdit(const Point &point) const;
  short allocateControlId();
  void beginClip(const Rect &rect);
  void endClip();
  ToolboxNodeContextMapper *contextMapper() const;
  loka::app::scene::PlatformLayoutHandlerRegistry *layoutHandlerRegistry() { return &layoutHandlerRegistry_; }
  void setActiveLayoutBoundary(loka::app::scene::BoundaryNode *boundary) { activeLayoutBoundary_ = boundary; }
  loka::app::scene::BoundaryNode *activeLayoutBoundary() const { return activeLayoutBoundary_; }

private:
  struct ButtonHit
  {
    Rect rect;
    loka::core::EmitterState *emitter;
    loka::core::State<bool> *enabled;
    loka::app::scene::BoundaryNode *boundary;
    ToolboxButtonContext *context;
  };

  struct CellHit
  {
    Rect rect;
    loka::core::EmitterState *emitter;
    loka::app::scene::BoundaryNode *boundary;
    ToolboxCellContext *context;
    loka::core::State<loka::core::String> *text;
  };

  struct EditHit
  {
    Rect rect;
    loka::core::State<loka::core::String> *text;
    loka::app::scene::BoundaryNode *boundary;
  };

  struct TextHit
  {
    Rect rect;
    short x;
    short y;
    loka::core::State<loka::core::String> *text;
    loka::app::scene::BoundaryNode *boundary;
    short lastMeasuredWidth;
    bool needsRelayoutOnChange;
  };
  struct PopupHit
  {
    Rect rect;
    short lineHeight;
    const loka::Vector<loka::core::String> *items;
    loka::core::State<int> *selectedIndex;
    loka::core::EmitterState *onChange;
    loka::core::State<bool> *enabled;
    loka::app::scene::BoundaryNode *boundary;
    short menuId;
    ToolboxPopupMenuContext *context;
  };
  struct TextBinding
  {
    loka::core::State<loka::core::String> *state;
    ToolboxScenePlatformController *controller;
  };

  struct EnabledBinding
  {
    loka::core::State<bool> *state;
    ToolboxScenePlatformController *controller;
  };

  struct ButtonControlBinding
  {
    short resourceId;
    ControlRef control;
    loka::core::EmitterState *emitter;
    loka::core::State<bool> *enabled;
    bool usedThisFrame;
    bool needsDraw;
    Rect rect;
    std::string label;
  };

  struct EditTextControlBinding
  {
    loka::core::State<loka::core::String> *text;
    TEHandle te;
    Rect rect;
    bool usedThisFrame;
    std::string lastText;
  };

  ToolboxWindow *window_;
  loka::app::scene::Node *rootNode_;
  loka::app::scene::Node *pendingRootNode_;
  std::vector<ButtonHit> buttonHits_;
  std::vector<CellHit> cellHits_;
  std::vector<ButtonControlBinding> buttonControls_;
  std::vector<EditTextControlBinding> editControls_;
  std::vector<EditHit> editHits_;
  std::vector<PopupHit> popupHits_;
  loka::core::State<loka::core::String> *focusedText_;
  EditTextControlBinding *focusedEdit_;
  Rect focusedRect_;
  bool hasFocusedRect_;
  std::vector<TextHit> textHits_;
  std::vector<loka::core::State<loka::core::String> *> boundTextStates_;
  std::vector<TextBinding *> textBindings_;
  std::vector<loka::core::State<bool> *> boundEnabledStates_;
  std::vector<EnabledBinding *> enabledBindings_;
  bool inBatchUpdate_;
  bool pendingFullInvalidate_;
  loka::app::scene::NodeDirtyFlags pendingInvalidateFlags_;
  bool forceFullRedraw_;
  std::vector<Rect> pendingDirtyRects_;
  std::vector<loka::core::State<loka::core::String> *> pendingTextStates_;
  std::vector<ControlRef> retiredControls_;
  std::vector<TEHandle> retiredTextEdits_;
  RgnHandle clipRgn_;
  bool hasClip_;
  short nextControlId_;
  ToolboxSceneDebugStats debugStats_;
  loka::app::scene::PlatformLayoutHandlerRegistry layoutHandlerRegistry_;
  loka::app::scene::BoundaryNode *activeLayoutBoundary_;

  bool handleTextKey(char key);
  void bindTextState(loka::core::State<loka::core::String> *text);
  void bindEnabledState(loka::core::State<bool> *enabled);
  void handleTextChanged(loka::core::State<loka::core::String> *text);
  void handleEnabledChanged(loka::core::State<bool> *enabled);
  void beginBatchUpdate();
  void endBatchUpdate();
  void addPendingDirty(const Rect &rect);
  void addPendingText(loka::core::State<loka::core::String> *text);
  bool collectLocalBoundaryDirtyRects(loka::app::scene::Node *node, const Rect &fallback);
  void requestInvalidateForChange(loka::app::scene::Node *rootNodeForChange,
                                  loka::app::scene::NodeDirtyFlags flags,
                                  bool fullRebuild);
  void redrawTextHit(TextHit &hit);
  void redrawPopupHit(const PopupHit &hit);
  void redrawTextFor(loka::core::State<loka::core::String> *text);
  void clearTextBindings();
  void clearEnabledBindings();
  void clearControls();
  void queueRetiredControl(ControlRef control);
  void queueRetiredTextEdit(TEHandle te);
  void syncEditTextFromState(EditTextControlBinding &binding);
  void updateStateFromEdit(EditTextControlBinding &binding);
  static void TextStateChangedThunk(void *userData);
  static void EnabledStateChangedThunk(void *userData);
public:
  void flushRetiredNativeHandles();
  std::string debugStatsSummary() const;
  void noteWindowDraw() { ++debugStats_.drawCalls; ++debugStats_.totalDrawCalls; }
  void noteWindowDirtyDraw() { ++debugStats_.drawDirtyCalls; ++debugStats_.totalDrawDirtyCalls; }
  void noteWindowFullRequest(const char *reason)
  {
    ++debugStats_.windowFullRequestCount;
    debugStats_.windowFullRequestSource = reason;
  }
  void noteWindowRectRequest() { ++debugStats_.windowRectRequestCount; }
  void noteWindowFlushFull() { ++debugStats_.windowFlushFullCount; }
  void noteWindowFlushDirty() { ++debugStats_.windowFlushDirtyCount; }
  void noteWindowUpdateEvtDraw() { ++debugStats_.windowUpdateEvtDrawCount; }
  void resetDebugStats();
  bool dumpDebugStatsToTimestampedFile() const;
};

#endif // LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
