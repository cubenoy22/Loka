#ifndef LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP

#include "core2/scene/PlatformController.hpp"
#include "core/State.hpp"
#include "loka/core/String.hpp"
#include <Quickdraw.h>
#include <Controls.h>
#include <TextEdit.h>

class ToolboxWindow;
class ToolboxPopupMenuContext;
class ToolboxNodeContextMapper;

class ToolboxScenePlatformController : public declara::core::scene::IPlatformController
{
public:
  explicit ToolboxScenePlatformController(ToolboxWindow *window);
  virtual ~ToolboxScenePlatformController();

  virtual void onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags flags);
  virtual void synchronize();
  virtual void destroy();

  void render();
  bool handleMouseDown(const Point &point);
  void recordButtonHit(const Rect &rect, declara::core::EmitterState *emitter, declara::core::State<bool> *enabled);
  void recordEditHit(const Rect &rect, declara::core::State<loka::core::String> *text);
  void recordTextHit(const Rect &rect, short x, short y, declara::core::State<loka::core::String> *text);
  void registerPopupContext(ToolboxPopupMenuContext *context);
  void applyPopupSelectionChange(const Rect &rect,
                                 declara::core::State<int> *selectedIndex,
                                 declara::core::EmitterState *onChange,
                                 int newIndex);
  bool handleKeyDown(char key);
  bool handleControlClick(const Point &point);
  void drawControlsInRect(const Rect &rect);
  bool ensureButtonControl(short resourceId, const Rect &rect, const loka::core::String &label, declara::core::EmitterState *emitter);
  void drawFallbackControl(const Rect &rect);
  TEHandle ensureEditTextControl(const Rect &rect, declara::core::State<loka::core::String> *text);
  void idleTextEdits();
  bool isPointInEdit(const Point &point) const;
  void beginClip(const Rect &rect);
  void endClip();

private:
  struct ButtonHit
  {
    Rect rect;
    declara::core::EmitterState *emitter;
    declara::core::State<bool> *enabled;
  };

  struct EditHit
  {
    Rect rect;
    declara::core::State<loka::core::String> *text;
  };

  struct TextHit
  {
    Rect rect;
    short x;
    short y;
    declara::core::State<loka::core::String> *text;
  };
  struct TextBinding
  {
    declara::core::State<loka::core::String> *state;
    ToolboxScenePlatformController *controller;
  };

  struct ButtonControlBinding
  {
    short resourceId;
    ControlRef control;
    declara::core::EmitterState *emitter;
    bool usedThisFrame;
    bool needsDraw;
    Rect rect;
    std::string label;
  };

  struct EditTextControlBinding
  {
    declara::core::State<loka::core::String> *text;
    TEHandle te;
    Rect rect;
    bool usedThisFrame;
    std::string lastText;
  };

  ToolboxWindow *window_;
  declara::core::scene::Node *rootNode_;
  std::vector<ButtonHit> buttonHits_;
  std::vector<ButtonControlBinding> buttonControls_;
  std::vector<EditTextControlBinding> editControls_;
  std::vector<EditHit> editHits_;
  std::vector<ToolboxPopupMenuContext *> popupContexts_;
  declara::core::State<loka::core::String> *focusedText_;
  EditTextControlBinding *focusedEdit_;
  Rect focusedRect_;
  bool hasFocusedRect_;
  std::vector<TextHit> textHits_;
  std::vector<declara::core::State<loka::core::String> *> boundTextStates_;
  std::vector<TextBinding *> textBindings_;
  bool inBatchUpdate_;
  std::vector<Rect> pendingDirtyRects_;
  std::vector<declara::core::State<loka::core::String> *> pendingTextStates_;
  RgnHandle clipRgn_;
  bool hasClip_;
  ToolboxNodeContextMapper *contextMapper_;

  bool handleTextKey(char key);
  void bindTextState(declara::core::State<loka::core::String> *text);
  void handleTextChanged(declara::core::State<loka::core::String> *text);
  void beginBatchUpdate();
  void endBatchUpdate();
  void addPendingDirty(const Rect &rect);
  void addPendingText(declara::core::State<loka::core::String> *text);
  void redrawTextHit(const TextHit &hit);
  void redrawTextFor(declara::core::State<loka::core::String> *text);
  void clearTextBindings();
  void clearControls();
  void syncEditTextFromState(EditTextControlBinding &binding);
  void updateStateFromEdit(EditTextControlBinding &binding);
  static void TextStateChangedThunk(void *userData);
};

#endif // LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
