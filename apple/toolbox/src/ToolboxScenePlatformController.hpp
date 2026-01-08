#ifndef LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP

#include "core2/scene/PlatformController.hpp"
#include "core/State.hpp"
#include "loka/core/String.hpp"
#include <Quickdraw.h>
#include <string>
#include <Controls.h>
#include <vector>

class ToolboxWindow;

class ToolboxScenePlatformController : public declara::core::scene::IPlatformController
{
public:
  explicit ToolboxScenePlatformController(ToolboxWindow *window);
  virtual ~ToolboxScenePlatformController();

  virtual void onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags flags);
  virtual void synchronize();
  virtual void destroy();

  void render();
  void handleMouseDown(const Point &point);
  void recordButtonHit(const Rect &rect, declara::core::EmitterState *emitter, declara::core::State<bool> *enabled);
  void recordEditHit(const Rect &rect, declara::core::State<loka::core::String> *text);
  void recordTextHit(const Rect &rect, declara::core::State<loka::core::String> *text);
  bool handleKeyDown(char key);
  bool handleControlClick(const Point &point);
  void drawControlsInRect(const Rect &rect);
  bool ensureButtonControl(short resourceId, const Rect &rect, const loka::core::String &label, declara::core::EmitterState *emitter);
  void drawFallbackControl(const Rect &rect);

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

  ToolboxWindow *window_;
  declara::core::scene::Node *rootNode_;
  std::vector<ButtonHit> buttonHits_;
  std::vector<ButtonControlBinding> buttonControls_;
  std::vector<EditHit> editHits_;
  declara::core::State<loka::core::String> *focusedText_;
  Rect focusedRect_;
  bool hasFocusedRect_;
  std::vector<EditHit> textHits_;
  std::vector<declara::core::State<loka::core::String> *> boundTextStates_;
  std::vector<TextBinding *> textBindings_;
  bool inBatchUpdate_;
  std::vector<Rect> pendingDirtyRects_;

  bool handleTextKey(char key);
  void bindTextState(declara::core::State<loka::core::String> *text);
  void handleTextChanged(declara::core::State<loka::core::String> *text);
  void beginBatchUpdate();
  void endBatchUpdate();
  void addPendingDirty(const Rect &rect);
  void clearTextBindings();
  void clearControls();
  static void TextStateChangedThunk(void *userData);
};

#endif // LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
