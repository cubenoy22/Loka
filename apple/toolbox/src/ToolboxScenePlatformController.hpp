#ifndef LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP

#include "app/scene/PlatformController.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include "app/scene/node/Boundary.hpp"
#include <Quickdraw.h>
#include <Controls.h>
#include <TextEdit.h>

class ToolboxWindow;
class ToolboxPopupMenuContext;
class ToolboxNodeContextMapper;
class ToolboxCellContext;

class ToolboxScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  explicit ToolboxScenePlatformController(ToolboxWindow *window);
  virtual ~ToolboxScenePlatformController();

  virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild);
  virtual void synchronize();
  virtual bool hasPendingSync() const;
  virtual void destroy();

  void render();
  void renderDirty(const Rect &rect);
  bool handleMouseDown(const Point &point);
  void recordButtonHit(const Rect &rect,
                       loka::core::EmitterState *emitter,
                       loka::core::State<bool> *enabled,
                       loka::app::scene::BoundaryNode *boundary);
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
                     bool needsRelayoutOnChange);
  void recordPopupHit(const Rect &rect,
                      short lineHeight,
                      const loka::Vector<loka::core::String> *items,
                      loka::core::State<int> *selectedIndex,
                      loka::core::EmitterState *onChange,
                      loka::core::State<bool> *enabled,
                      loka::app::scene::BoundaryNode *boundary,
                      short menuId);
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

private:
  struct ButtonHit
  {
    Rect rect;
    loka::core::EmitterState *emitter;
    loka::core::State<bool> *enabled;
    loka::app::scene::BoundaryNode *boundary;
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
  };
  struct TextBinding
  {
    loka::core::State<loka::core::String> *state;
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

  struct DebugStats
  {
    DebugStats()
        : changeSequence(0),
          totalChanges(0),
          lastFlags(loka::app::scene::NODE_DIRTY_NONE),
          lastFullRebuild(false),
          lastRootPresent(false),
          fullInvalidateRequests(0),
          rectInvalidateRequests(0),
          drawCalls(0),
          drawDirtyCalls(0),
          renderCalls(0),
          renderDirtyCalls(0),
          buttonHitCount(0),
          cellHitCount(0),
          editHitCount(0),
          textHitCount(0),
          popupHitCount(0),
          controlDrawCount(0),
          relayoutTextCount(0),
          textChangedCellCount(0),
          textChangedTextCount(0),
          textChangedEditHitCount(0),
          textChangedEditControlCount(0),
          textChangedPendingCount(0),
          textChangedImmediateInvalidateCount(0),
          batchOnChangeCount(0),
          batchNullRootCount(0),
          batchFullRebuildCount(0),
          batchNonNoneFlagsCount(0),
          batchAccumOnChangeCount(0),
          batchAccumNullRootCount(0),
          batchAccumFullRebuild(false),
          batchAccumFlags(loka::app::scene::NODE_DIRTY_NONE),
          batchAccumTrace(),
          fallbackRootIsBoundary(false),
          fallbackRootHasLayoutBounds(false),
          fallbackQueuedByChild(false),
          fallbackUsedFullInvalidate(false),
          windowFullRequestCount(0),
          windowRectRequestCount(0),
          windowFlushFullCount(0),
          windowFlushDirtyCount(0),
          windowUpdateEvtDrawCount(0),
          windowFullRequestSource(0),
          requestInvalidateCallCount(0),
          requestInvalidateFirstRootPresent(false),
          requestInvalidateFirstFullRebuild(false),
          requestInvalidateFirstFlags(loka::app::scene::NODE_DIRTY_NONE),
          requestInvalidateRootPresent(false),
          requestInvalidateFullRebuild(false),
          requestInvalidateFlags(loka::app::scene::NODE_DIRTY_NONE),
          totalFullInvalidateRequests(0),
          totalRectInvalidateRequests(0),
          totalDrawCalls(0),
          totalDrawDirtyCalls(0),
          totalRenderCalls(0),
          totalRenderDirtyCalls(0),
          totalControlDrawCount(0)
    {
    }

    unsigned long changeSequence;
    unsigned long totalChanges;
    loka::app::scene::NodeDirtyFlags lastFlags;
    bool lastFullRebuild;
    bool lastRootPresent;
    int fullInvalidateRequests;
    int rectInvalidateRequests;
    int drawCalls;
    int drawDirtyCalls;
    int renderCalls;
    int renderDirtyCalls;
    int buttonHitCount;
    int cellHitCount;
    int editHitCount;
    int textHitCount;
    int popupHitCount;
    int controlDrawCount;
    int relayoutTextCount;
    int textChangedCellCount;
    int textChangedTextCount;
    int textChangedEditHitCount;
    int textChangedEditControlCount;
    int textChangedPendingCount;
    int textChangedImmediateInvalidateCount;
    int batchOnChangeCount;
    int batchNullRootCount;
    int batchFullRebuildCount;
    int batchNonNoneFlagsCount;
    int batchAccumOnChangeCount;
    int batchAccumNullRootCount;
    bool batchAccumFullRebuild;
    loka::app::scene::NodeDirtyFlags batchAccumFlags;
    std::string batchAccumTrace;
    std::string relayoutTextPreview;
    bool fallbackRootIsBoundary;
    bool fallbackRootHasLayoutBounds;
    bool fallbackQueuedByChild;
    bool fallbackUsedFullInvalidate;
    int windowFullRequestCount;
    int windowRectRequestCount;
    int windowFlushFullCount;
    int windowFlushDirtyCount;
    int windowUpdateEvtDrawCount;
    const char *windowFullRequestSource;
    int requestInvalidateCallCount;
    bool requestInvalidateFirstRootPresent;
    bool requestInvalidateFirstFullRebuild;
    loka::app::scene::NodeDirtyFlags requestInvalidateFirstFlags;
    bool requestInvalidateRootPresent;
    bool requestInvalidateFullRebuild;
    loka::app::scene::NodeDirtyFlags requestInvalidateFlags;
    int totalFullInvalidateRequests;
    int totalRectInvalidateRequests;
    int totalDrawCalls;
    int totalDrawDirtyCalls;
    int totalRenderCalls;
    int totalRenderDirtyCalls;
    int totalControlDrawCount;
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
  DebugStats debugStats_;

  bool handleTextKey(char key);
  void bindTextState(loka::core::State<loka::core::String> *text);
  void handleTextChanged(loka::core::State<loka::core::String> *text);
  void beginBatchUpdate();
  void endBatchUpdate();
  void addPendingDirty(const Rect &rect);
  void addPendingText(loka::core::State<loka::core::String> *text);
  bool collectLocalBoundaryDirtyRects(loka::app::scene::Node *node, const Rect &fallback);
  void requestInvalidateForChange(loka::app::scene::Node *rootNodeForChange,
                                  loka::app::scene::NodeDirtyFlags flags,
                                  bool fullRebuild);
  void redrawTextHit(const TextHit &hit);
  void redrawPopupHit(const PopupHit &hit);
  void redrawTextFor(loka::core::State<loka::core::String> *text);
  void clearTextBindings();
  void clearControls();
  void queueRetiredControl(ControlRef control);
  void queueRetiredTextEdit(TEHandle te);
  void syncEditTextFromState(EditTextControlBinding &binding);
  void updateStateFromEdit(EditTextControlBinding &binding);
  static void TextStateChangedThunk(void *userData);
  void beginDebugStats(loka::app::scene::NodeDirtyFlags flags, bool fullRebuild);
  void refreshHitDebugStats();
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
