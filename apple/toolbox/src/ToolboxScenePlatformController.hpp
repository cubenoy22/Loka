#ifndef LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP

#include "core2/scene/PlatformController.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "core2/scene/node/Boundary.hpp"
#include <Quickdraw.h>
#include <Controls.h>
#include <TextEdit.h>

class ToolboxWindow;
class ToolboxPopupMenuContext;
class ToolboxNodeContextMapper;
class ToolboxCellContext;

class ToolboxScenePlatformController : public loka::core::scene::IPlatformController
{
public:
  explicit ToolboxScenePlatformController(ToolboxWindow *window);
  virtual ~ToolboxScenePlatformController();

  virtual void onChange(loka::core::scene::Node *rootNode, loka::core::scene::NodeDirtyFlags flags);
  virtual void synchronize();
  virtual void destroy();

  void render();
  void renderDirty(const Rect &rect);
  bool handleMouseDown(const Point &point);
  void recordButtonHit(const Rect &rect,
                       loka::core::EmitterState *emitter,
                       loka::core::State<bool> *enabled,
                       loka::core::scene::BoundaryNode *boundary);
  void recordCellHit(const Rect &rect,
                     loka::core::EmitterState *emitter,
                     loka::core::scene::BoundaryNode *boundary,
                     ToolboxCellContext *context,
                     loka::core::State<loka::core::String> *text);
  void recordEditHit(const Rect &rect,
                     loka::core::State<loka::core::String> *text,
                     loka::core::scene::BoundaryNode *boundary);
  void recordTextHit(const Rect &rect,
                     short x,
                     short y,
                     loka::core::State<loka::core::String> *text,
                     loka::core::scene::BoundaryNode *boundary);
  void registerPopupContext(ToolboxPopupMenuContext *context);
  void applyPopupSelectionChange(const Rect &rect,
                                 loka::core::scene::BoundaryNode *boundary,
                                 loka::core::State<int> *selectedIndex,
                                 loka::core::EmitterState *onChange,
                                 int newIndex);
  bool handleKeyDown(char key);
  bool handleControlClick(const Point &point);
  void drawControlsInRect(const Rect &rect);
  bool ensureButtonControl(short resourceId, const Rect &rect, const loka::core::String &label, loka::core::EmitterState *emitter);
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
    loka::core::scene::BoundaryNode *boundary;
  };

  struct CellHit
  {
    Rect rect;
    loka::core::EmitterState *emitter;
    loka::core::scene::BoundaryNode *boundary;
    ToolboxCellContext *context;
    loka::core::State<loka::core::String> *text;
  };

  struct EditHit
  {
    Rect rect;
    loka::core::State<loka::core::String> *text;
    loka::core::scene::BoundaryNode *boundary;
  };

  struct TextHit
  {
    Rect rect;
    short x;
    short y;
    loka::core::State<loka::core::String> *text;
    loka::core::scene::BoundaryNode *boundary;
    short lastMeasuredWidth;
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
  loka::core::scene::Node *rootNode_;
  std::vector<ButtonHit> buttonHits_;
  std::vector<CellHit> cellHits_;
  std::vector<ButtonControlBinding> buttonControls_;
  std::vector<EditTextControlBinding> editControls_;
  std::vector<EditHit> editHits_;
  std::vector<ToolboxPopupMenuContext *> popupContexts_;
  loka::core::State<loka::core::String> *focusedText_;
  EditTextControlBinding *focusedEdit_;
  Rect focusedRect_;
  bool hasFocusedRect_;
  std::vector<TextHit> textHits_;
  std::vector<loka::core::State<loka::core::String> *> boundTextStates_;
  std::vector<TextBinding *> textBindings_;
  bool inBatchUpdate_;
  bool pendingFullInvalidate_;
  bool forceFullRedraw_;
  std::vector<Rect> pendingDirtyRects_;
  std::vector<loka::core::State<loka::core::String> *> pendingTextStates_;
  RgnHandle clipRgn_;
  bool hasClip_;
  short nextControlId_;

  bool handleTextKey(char key);
  void bindTextState(loka::core::State<loka::core::String> *text);
  void handleTextChanged(loka::core::State<loka::core::String> *text);
  void beginBatchUpdate();
  void endBatchUpdate();
  void addPendingDirty(const Rect &rect);
  void addPendingText(loka::core::State<loka::core::String> *text);
  void redrawTextHit(const TextHit &hit);
  void redrawTextFor(loka::core::State<loka::core::String> *text);
  void clearTextBindings();
  void clearControls();
  void syncEditTextFromState(EditTextControlBinding &binding);
  void updateStateFromEdit(EditTextControlBinding &binding);
  static void TextStateChangedThunk(void *userData);
};

#endif // LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
