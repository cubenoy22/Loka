#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include <Quickdraw.h>
#include <cstring>
#include <string>
#include <Memory.h>
#include <Menus.h>
#include "loka/platform/StringUTF8.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "loka/core/String.hpp"
#include "app/Text.hpp"
#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "context/ToolboxNodeContextMapper.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "core2/scene/Node.hpp"

namespace
{
  struct RenderState
  {
    short x;
    short y;
    short lineHeight;
    short spacing;
  };

  void DrawStringAt(short x, short y, const loka::core::String &value)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return;
    }
    std::size_t length = utf8.size();
    if (length > 255)
    {
      length = 255;
    }
    Str255 text;
    text[0] = static_cast<unsigned char>(length);
    if (length > 0)
    {
      std::memcpy(text + 1, utf8.data(), length);
    }
    MoveTo(x, y);
    DrawString(text);
  }

  short MeasureTextWidth(const loka::core::String &value)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return 40;
    }
    return static_cast<short>(utf8.size() * 7 + 16);
  }

  void CopyToPascalString(const loka::core::String &value, Str255 out)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      out[0] = 0;
      return;
    }
    std::size_t length = utf8.size();
    if (length > 255)
    {
      length = 255;
    }
    out[0] = static_cast<unsigned char>(length);
    if (length > 0)
    {
      std::memcpy(out + 1, utf8.data(), length);
    }
  }

  short LayoutNode(declara::core::scene::Node *node,
                   RenderState &state,
                   ToolboxScenePlatformController *controller);
  void RenderNode(declara::core::scene::Node *node,
                  ToolboxScenePlatformController *controller);

  short LayoutChildren(declara::core::scene::INestable *nestable,
                       RenderState &state,
                       ToolboxScenePlatformController *controller)
  {
    if (!nestable)
    {
      return 0;
    }
    short maxWidth = 0;
    loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (declara::core::scene::Node *child = it.next(); child; child = it.next())
    {
      short width = LayoutNode(child, state, controller);
      if (width > maxWidth)
      {
        maxWidth = width;
      }
    }
    return maxWidth;
  }

  short LayoutNode(declara::core::scene::Node *node,
                   RenderState &state,
                   ToolboxScenePlatformController *controller)
  {
    if (!node)
    {
      return 0;
    }
    switch (node->kind())
    {
    case declara::core::scene::NODE_KIND_COLUMN:
      return LayoutChildren(node->asNestable(), state, controller);
    case declara::core::scene::NODE_KIND_ROW:
    {
      declara::core::scene::NestableNode *nestable = static_cast<declara::core::scene::NestableNode *>(node);
      short startX = state.x;
      short maxHeight = 0;
      loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (declara::core::scene::Node *child = it.next(); child; child = it.next())
      {
        RenderState rowState = state;
        rowState.x = startX;
        short width = LayoutNode(child, rowState, controller);
        startX = static_cast<short>(startX + width + state.spacing);
        if (rowState.y > state.y)
        {
          maxHeight = static_cast<short>(rowState.y - state.y);
        }
      }
      state.y = static_cast<short>(state.y + maxHeight + state.spacing);
      return static_cast<short>(startX - state.x);
    }
    case declara::core::scene::NODE_KIND_TEXT:
    {
      declara::app::TextNode *text = static_cast<declara::app::TextNode *>(node);
      if (text->props.text_)
      {
        short width = MeasureTextWidth(text->props.text_->get());
        Rect rect;
        rect.left = state.x;
        rect.top = static_cast<short>(state.y - state.lineHeight + 2);
        rect.right = static_cast<short>(state.x + width);
        rect.bottom = static_cast<short>(state.y + 6);
        if (controller && controller->contextMapper())
        {
          controller->contextMapper()->ensureTextContext(text, rect, state.x, state.y, controller);
        }
        state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
        return width;
      }
      return 0;
    }
    case declara::core::scene::NODE_KIND_BUTTON:
    {
      declara::app::ButtonNode *button = static_cast<declara::app::ButtonNode *>(node);
      loka::core::String label = loka::core::String::Literal("Button");
      if (button->props.text_)
      {
        label = button->props.text_->get();
      }
      short width = MeasureTextWidth(label);
      Rect rect;
      rect.left = state.x;
      rect.top = static_cast<short>(state.y - state.lineHeight + 2);
      rect.right = static_cast<short>(state.x + width);
      rect.bottom = static_cast<short>(state.y + 6);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensureButtonContext(button, rect, label, controller);
      }
      state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
      return width;
    }
    case declara::core::scene::NODE_KIND_EDIT_TEXT:
    {
      declara::app::EditTextNode *edit = static_cast<declara::app::EditTextNode *>(node);
      short width = 120;
      Rect rect;
      rect.left = state.x;
      rect.top = static_cast<short>(state.y - state.lineHeight + 2);
      rect.right = static_cast<short>(state.x + width + 3);
      rect.bottom = static_cast<short>(state.y + 6 + 2);
      Rect textRect = rect;
      textRect.left = static_cast<short>(textRect.left + 1);
      textRect.top = static_cast<short>(textRect.top + 2);
      textRect.right = static_cast<short>(textRect.right - 1);
      textRect.bottom = static_cast<short>(textRect.bottom - 1);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensureEditTextContext(edit, rect, textRect, static_cast<short>(state.x + 4), state.y, controller);
      }
      state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
      return width;
    }
    case declara::core::scene::NODE_KIND_POPUP_MENU:
    {
      declara::app::PopupMenuNode *popup = static_cast<declara::app::PopupMenuNode *>(node);
      short width = 120;
      Rect rect;
      rect.left = state.x;
      rect.top = static_cast<short>(state.y - state.lineHeight + 2);
      rect.right = static_cast<short>(state.x + width + 8);
      rect.bottom = static_cast<short>(state.y + 6);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensurePopupMenuContext(popup, rect, state.lineHeight, controller);
      }
      state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
      return width;
    }
    default:
      break;
    }
    return LayoutChildren(node->asNestable(), state, controller);
  }

  void RenderChildren(declara::core::scene::INestable *nestable,
                      ToolboxScenePlatformController *controller)
  {
    if (!nestable)
    {
      return;
    }
    loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (declara::core::scene::Node *child = it.next(); child; child = it.next())
    {
      RenderNode(child, controller);
    }
  }

  void RenderNode(declara::core::scene::Node *node,
                  ToolboxScenePlatformController *controller)
  {
    if (!node)
    {
      return;
    }
    switch (node->kind())
    {
    case declara::core::scene::NODE_KIND_COLUMN:
      RenderChildren(node->asNestable(), controller);
      return;
    case declara::core::scene::NODE_KIND_ROW:
      RenderChildren(node->asNestable(), controller);
      return;
    case declara::core::scene::NODE_KIND_TEXT:
    {
      declara::app::TextNode *text = static_cast<declara::app::TextNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->renderTextContext(text, controller);
      }
      return;
    }
    case declara::core::scene::NODE_KIND_BUTTON:
    {
      declara::app::ButtonNode *button = static_cast<declara::app::ButtonNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->renderButtonContext(button, controller);
      }
      return;
    }
    case declara::core::scene::NODE_KIND_EDIT_TEXT:
    {
      declara::app::EditTextNode *edit = static_cast<declara::app::EditTextNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->renderEditTextContext(edit, controller);
      }
      return;
    }
    case declara::core::scene::NODE_KIND_POPUP_MENU:
    {
      declara::app::PopupMenuNode *popup = static_cast<declara::app::PopupMenuNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->renderPopupMenuContext(popup);
      }
      return;
    }
    default:
      break;
    }
    RenderChildren(node->asNestable(), controller);
  }
}

ToolboxScenePlatformController::ToolboxScenePlatformController(ToolboxWindow *window)
    : window_(window),
      rootNode_(0),
      focusedText_(0),
      focusedEdit_(0),
      focusedRect_(),
      hasFocusedRect_(false),
      inBatchUpdate_(false),
      pendingDirtyRects_(),
      clipRgn_(NewRgn()),
      hasClip_(false),
      contextMapper_(new ToolboxNodeContextMapper(
#if !defined(DECLARA_TOOLBOX_CLASSIC_6)
          ToolboxNodeContextMapper::CAP_CONTROL_MANAGER | ToolboxNodeContextMapper::CAP_TEXT_EDIT
#else
          ToolboxNodeContextMapper::CAP_NONE
#endif
      ))
{
}

ToolboxScenePlatformController::~ToolboxScenePlatformController()
{
  clearTextBindings();
  clearControls();
  delete contextMapper_;
  contextMapper_ = 0;
  if (clipRgn_)
  {
    DisposeRgn(clipRgn_);
    clipRgn_ = 0;
  }
}

void ToolboxScenePlatformController::onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags)
{
  rootNode_ = rootNode;
  if (!window_ || !window_->window())
  {
    return;
  }
  window_->requestInvalidate();
}

void ToolboxScenePlatformController::synchronize()
{
  // Toolbox doesn't have a retained scene graph; rely on Update events.
}

void ToolboxScenePlatformController::destroy()
{
  rootNode_ = 0;
  popupContexts_.clear();
  clearTextBindings();
  clearControls();
}

void ToolboxScenePlatformController::render()
{
  if (!window_ || !window_->window() || !rootNode_)
  {
    return;
  }
  buttonHits_.clear();
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    buttonControls_[i].usedThisFrame = false;
  }
  editHits_.clear();
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    editControls_[i].usedThisFrame = false;
  }
      textHits_.clear();
      popupContexts_.clear();
      pendingTextStates_.clear();
      pendingDirtyRects_.clear();
  RenderState state;
  state.x = 12;
  state.y = 24;
  state.lineHeight = 14;
  state.spacing = 6;
  LayoutNode(rootNode_, state, this);
  RenderNode(rootNode_, this);
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    if (!buttonControls_[i].usedThisFrame && buttonControls_[i].control)
    {
      HideControl(buttonControls_[i].control);
    }
  }
  for (size_t i = 0; i < editControls_.size();)
  {
    if (!editControls_[i].usedThisFrame)
    {
      if (editControls_[i].te)
      {
        TEDispose(editControls_[i].te);
      }
      if (&editControls_[i] == focusedEdit_)
      {
        focusedEdit_ = 0;
      }
      editControls_.erase(editControls_.begin() + i);
      continue;
    }
    ++i;
  }
}

bool ToolboxScenePlatformController::handleMouseDown(const Point &point)
{
  if (handleControlClick(point))
  {
    return false;
  }
  if (focusedEdit_ && focusedEdit_->te)
  {
    TEDeactivate(focusedEdit_->te);
    focusedEdit_ = 0;
  }
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    EditTextControlBinding &binding = editControls_[i];
    if (binding.te && PtInRect(point, &binding.rect))
    {
      focusedEdit_ = &binding;
      TEActivate(binding.te);
      TEClick(point, false, binding.te);
      return true;
    }
  }
  for (size_t i = 0; i < editHits_.size(); ++i)
  {
    EditHit &hit = editHits_[i];
    if (hit.text && PtInRect(point, &hit.rect))
    {
      focusedText_ = hit.text;
      focusedRect_ = hit.rect;
      hasFocusedRect_ = true;
      return true;
    }
  }
  focusedText_ = 0;
  hasFocusedRect_ = false;
  for (size_t i = 0; i < popupContexts_.size(); ++i)
  {
    ToolboxPopupMenuContext *ctx = popupContexts_[i];
    if (ctx && ctx->handleMouseDown(point, this))
    {
      return false;
    }
  }
  for (size_t i = 0; i < buttonHits_.size(); ++i)
  {
    ButtonHit &hit = buttonHits_[i];
    if (!hit.emitter)
    {
      continue;
    }
    if (hit.enabled && !hit.enabled->get())
    {
      continue;
    }
    if (PtInRect(point, &hit.rect))
    {
      beginBatchUpdate();
      hit.emitter->emit();
      endBatchUpdate();
      return false;
    }
  }
  return false;
}

bool ToolboxScenePlatformController::handleKeyDown(char key)
{
  if (focusedEdit_ && focusedEdit_->te)
  {
    beginBatchUpdate();
    TEKey(key, focusedEdit_->te);
    updateStateFromEdit(*focusedEdit_);
    endBatchUpdate();
    return true;
  }
  beginBatchUpdate();
  if (!handleTextKey(key))
  {
    endBatchUpdate();
    return false;
  }
  endBatchUpdate();
  return true;
}

void ToolboxScenePlatformController::recordButtonHit(const Rect &rect,
                                                     declara::core::EmitterState *emitter,
                                                     declara::core::State<bool> *enabled)
{
  if (!emitter)
  {
    return;
  }
  ButtonHit hit;
  hit.rect = rect;
  hit.emitter = emitter;
  hit.enabled = enabled;
  buttonHits_.push_back(hit);
}

void ToolboxScenePlatformController::recordEditHit(const Rect &rect, declara::core::State<loka::core::String> *text)
{
  EditHit hit;
  hit.rect = rect;
  hit.text = text;
  editHits_.push_back(hit);
  bindTextState(text);
  if (text && focusedText_ == text)
  {
    focusedRect_ = rect;
    hasFocusedRect_ = true;
  }
}

void ToolboxScenePlatformController::recordTextHit(const Rect &rect,
                                                   short x,
                                                   short y,
                                                   declara::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return;
  }
  TextHit hit;
  hit.rect = rect;
  hit.x = x;
  hit.y = y;
  hit.text = text;
  textHits_.push_back(hit);
  bindTextState(text);
}

void ToolboxScenePlatformController::registerPopupContext(ToolboxPopupMenuContext *context)
{
  if (context)
  {
    popupContexts_.push_back(context);
  }
}

void ToolboxScenePlatformController::applyPopupSelectionChange(const Rect &rect,
                                                               declara::core::State<int> *selectedIndex,
                                                               declara::core::EmitterState *onChange,
                                                               int newIndex)
{
  if (!selectedIndex)
  {
    return;
  }
  MutableState<int> *mutableIndex = dynamic_cast<MutableState<int> *>(selectedIndex);
  if (!mutableIndex)
  {
    return;
  }
  beginBatchUpdate();
  mutableIndex->set(newIndex, true);
  if (onChange)
  {
    onChange->emit();
  }
  endBatchUpdate();
  if (window_)
  {
    window_->drawDirty(rect);
  }
  for (size_t k = 0; k < textHits_.size(); ++k)
  {
    redrawTextHit(textHits_[k]);
  }
}

bool ToolboxScenePlatformController::handleTextKey(char key)
{
  if (!focusedText_)
  {
    return false;
  }
  declara::core::MutableState<loka::core::String> *mutableText =
      dynamic_cast<declara::core::MutableState<loka::core::String> *>(focusedText_);
  if (!mutableText)
  {
    return false;
  }
  std::string utf8;
  loka::platform::CollectUtf8(focusedText_->get(), utf8);
  if (key == 8 || key == 0x7F)
  {
    if (!utf8.empty())
    {
      utf8.erase(utf8.size() - 1);
    }
  }
  else if (key == 13)
  {
    return true;
  }
  else if (key >= 32)
  {
    utf8.push_back(key);
  }
  else
  {
    return false;
  }
  StateTrackerGuard _(window_ ? window_->getTracker() : 0);
  mutableText->set(loka::core::String(utf8));
  return true;
}

void ToolboxScenePlatformController::bindTextState(declara::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return;
  }
  for (size_t i = 0; i < boundTextStates_.size(); ++i)
  {
    if (boundTextStates_[i] == text)
    {
      return;
    }
  }
  boundTextStates_.push_back(text);
  TextBinding *binding = new TextBinding();
  binding->state = text;
  binding->controller = this;
  textBindings_.push_back(binding);
  text->bind(&ToolboxScenePlatformController::TextStateChangedThunk, binding, false, false, 0);
}

void ToolboxScenePlatformController::handleTextChanged(declara::core::State<loka::core::String> *text)
{
  if (!window_)
  {
    return;
  }
  for (size_t i = 0; i < textHits_.size(); ++i)
  {
    TextHit &hit = textHits_[i];
    if (hit.text == text)
    {
      if (inBatchUpdate_)
      {
        addPendingText(text);
      }
      else
      {
        redrawTextHit(hit);
      }
      return;
    }
  }
  for (size_t i = 0; i < editHits_.size(); ++i)
  {
    EditHit &hit = editHits_[i];
    if (hit.text == text)
    {
      if (inBatchUpdate_)
      {
        addPendingDirty(hit.rect);
      }
      else
      {
        window_->drawDirty(hit.rect);
      }
      return;
    }
  }
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].text == text)
    {
      if (!inBatchUpdate_)
      {
        syncEditTextFromState(editControls_[i]);
        window_->drawDirty(editControls_[i].rect);
      }
      return;
    }
  }
  if (inBatchUpdate_)
  {
    addPendingDirty(window_->window()->portRect);
  }
  else
  {
    window_->draw();
  }
}

void ToolboxScenePlatformController::beginBatchUpdate()
{
  inBatchUpdate_ = true;
  pendingDirtyRects_.clear();
  pendingTextStates_.clear();
}

void ToolboxScenePlatformController::endBatchUpdate()
{
  inBatchUpdate_ = false;
  if (window_)
  {
    for (size_t i = 0; i < pendingDirtyRects_.size(); ++i)
    {
      window_->drawDirty(pendingDirtyRects_[i]);
    }
    for (size_t i = 0; i < pendingTextStates_.size(); ++i)
    {
      redrawTextFor(pendingTextStates_[i]);
    }
  }
  pendingDirtyRects_.clear();
  pendingTextStates_.clear();
}

void ToolboxScenePlatformController::addPendingDirty(const Rect &rect)
{
  for (size_t i = 0; i < pendingDirtyRects_.size(); ++i)
  {
    Rect &pending = pendingDirtyRects_[i];
    if (rect.right < pending.left || rect.left > pending.right ||
        rect.bottom < pending.top || rect.top > pending.bottom)
    {
      continue;
    }
    if (rect.left < pending.left)
    {
      pending.left = rect.left;
    }
    if (rect.top < pending.top)
    {
      pending.top = rect.top;
    }
    if (rect.right > pending.right)
    {
      pending.right = rect.right;
    }
    if (rect.bottom > pending.bottom)
    {
      pending.bottom = rect.bottom;
    }
    return;
  }
  pendingDirtyRects_.push_back(rect);
}

void ToolboxScenePlatformController::addPendingText(declara::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return;
  }
  for (size_t i = 0; i < pendingTextStates_.size(); ++i)
  {
    if (pendingTextStates_[i] == text)
    {
      return;
    }
  }
  pendingTextStates_.push_back(text);
}

void ToolboxScenePlatformController::redrawTextHit(const TextHit &hit)
{
  if (!window_ || !window_->window() || !hit.text)
  {
    return;
  }
  GrafPtr oldPort;
  GetPort(&oldPort);
  SetPort(window_->window());
  EraseRect(&hit.rect);
  DrawStringAt(hit.x, hit.y, hit.text->get());
  SetPort(oldPort);
}

void ToolboxScenePlatformController::redrawTextFor(declara::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return;
  }
  for (size_t i = 0; i < textHits_.size(); ++i)
  {
    if (textHits_[i].text == text)
    {
      redrawTextHit(textHits_[i]);
      return;
    }
  }
}

void ToolboxScenePlatformController::clearTextBindings()
{
  for (size_t i = 0; i < textBindings_.size(); ++i)
  {
    TextBinding *binding = textBindings_[i];
    if (binding && binding->state)
    {
      binding->state->unbind(&ToolboxScenePlatformController::TextStateChangedThunk, binding);
    }
    delete binding;
  }
  textBindings_.clear();
  boundTextStates_.clear();
  textHits_.clear();
}

void ToolboxScenePlatformController::clearControls()
{
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    if (buttonControls_[i].control)
    {
      DisposeControl(buttonControls_[i].control);
    }
  }
  buttonControls_.clear();
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].te)
    {
      TEDispose(editControls_[i].te);
    }
  }
  editControls_.clear();
  focusedEdit_ = 0;
}

bool ToolboxScenePlatformController::ensureButtonControl(
    short resourceId,
    const Rect &rect,
    const loka::core::String &label,
    declara::core::EmitterState *emitter)
{
  if (!window_ || !window_->window() || resourceId <= 0)
  {
    return false;
  }
  ButtonControlBinding *binding = 0;
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    if (buttonControls_[i].resourceId == resourceId)
    {
      binding = &buttonControls_[i];
      break;
    }
  }
  bool created = false;
  if (!binding)
  {
    ControlRef control = GetNewControl(resourceId, window_->window());
    if (!control)
    {
      return false;
    }
    HideControl(control);
    ButtonControlBinding entry;
    entry.resourceId = resourceId;
    entry.control = control;
    entry.emitter = emitter;
    entry.usedThisFrame = true;
    entry.needsDraw = true;
    entry.rect = rect;
    entry.label = "";
    buttonControls_.push_back(entry);
    binding = &buttonControls_.back();
    created = true;
  }
  binding->emitter = emitter;
  binding->usedThisFrame = true;
  if (created ||
      binding->rect.left != rect.left || binding->rect.top != rect.top ||
      binding->rect.right != rect.right || binding->rect.bottom != rect.bottom)
  {
    MoveControl(binding->control, rect.left, rect.top);
    SizeControl(binding->control, rect.right - rect.left, rect.bottom - rect.top);
    binding->rect = rect;
    binding->needsDraw = true;
  }
  std::string labelUtf8;
  if (!loka::platform::CollectUtf8(label, labelUtf8))
  {
    labelUtf8.clear();
  }
  if (binding->label != labelUtf8)
  {
    Str255 title;
    CopyToPascalString(label, title);
    SetControlTitle(binding->control, title);
    binding->label = labelUtf8;
    binding->needsDraw = true;
  }
  ShowControl(binding->control);
  return true;
}

void ToolboxScenePlatformController::drawFallbackControl(const Rect &rect)
{
  FrameRect(&rect);
  MoveTo(rect.left + 2, rect.top + 2);
  LineTo(rect.right - 2, rect.bottom - 2);
  MoveTo(rect.left + 2, rect.bottom - 2);
  LineTo(rect.right - 2, rect.top + 2);
}

TEHandle ToolboxScenePlatformController::ensureEditTextControl(
    const Rect &rect,
    declara::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return 0;
  }
  EditTextControlBinding *binding = 0;
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].text == text)
    {
      binding = &editControls_[i];
      break;
    }
  }
  if (!binding)
  {
    TEHandle te = TENew(&rect, &rect);
    if (!te)
    {
    return 0;
  }
    EditTextControlBinding entry;
    entry.text = text;
    entry.te = te;
    entry.rect = rect;
    entry.usedThisFrame = true;
    entry.lastText = "";
    editControls_.push_back(entry);
    binding = &editControls_.back();
    syncEditTextFromState(*binding);
    TEAutoView(true, binding->te);
  }
  binding->usedThisFrame = true;
  if (binding->rect.left != rect.left || binding->rect.top != rect.top ||
      binding->rect.right != rect.right || binding->rect.bottom != rect.bottom)
  {
    binding->rect = rect;
    if (binding->te)
    {
      (**binding->te).destRect = rect;
      (**binding->te).viewRect = rect;
      TECalText(binding->te);
      TEAutoView(true, binding->te);
    }
  }
  if (!inBatchUpdate_)
  {
    syncEditTextFromState(*binding);
  }
  return binding ? binding->te : 0;
}

void ToolboxScenePlatformController::syncEditTextFromState(EditTextControlBinding &binding)
{
  if (!binding.text || !binding.te)
  {
    return;
  }
  std::string utf8;
  loka::platform::CollectUtf8(binding.text->get(), utf8);
  if (binding.lastText == utf8)
  {
    return;
  }
  TESetText(utf8.c_str(), static_cast<long>(utf8.size()), binding.te);
  TESetSelect(utf8.size(), utf8.size(), binding.te);
  binding.lastText = utf8;
}

void ToolboxScenePlatformController::updateStateFromEdit(EditTextControlBinding &binding)
{
  if (!binding.text || !binding.te)
  {
    return;
  }
  declara::core::MutableState<loka::core::String> *mutableText =
      dynamic_cast<declara::core::MutableState<loka::core::String> *>(binding.text);
  if (!mutableText)
  {
    return;
  }
  CharsHandle textHandle = TEGetText(binding.te);
  long length = 0;
  if (binding.te && *binding.te)
  {
    length = (**binding.te).teLength;
  }
  std::string utf8;
  if (textHandle && length > 0)
  {
    HLock(reinterpret_cast<Handle>(textHandle));
    const char *ptr = *textHandle;
    utf8.assign(ptr, static_cast<size_t>(length));
    HUnlock(reinterpret_cast<Handle>(textHandle));
  }
  StateTrackerGuard _(window_ ? window_->getTracker() : 0);
  mutableText->set(loka::core::String(utf8));
  binding.lastText = utf8;
}

void ToolboxScenePlatformController::drawControlsInRect(const Rect &rect)
{
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    ButtonControlBinding &binding = buttonControls_[i];
    if (!binding.control || !binding.usedThisFrame)
    {
      continue;
    }
    if (rect.right < binding.rect.left || rect.left > binding.rect.right ||
        rect.bottom < binding.rect.top || rect.top > binding.rect.bottom)
    {
      continue;
    }
    Draw1Control(binding.control);
    binding.needsDraw = false;
  }
}

void ToolboxScenePlatformController::idleTextEdits()
{
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].te)
    {
      TEIdle(editControls_[i].te);
    }
  }
}

bool ToolboxScenePlatformController::isPointInEdit(const Point &point) const
{
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    const EditTextControlBinding &binding = editControls_[i];
    if (binding.te && PtInRect(point, &binding.rect))
    {
      return true;
    }
  }
  for (size_t i = 0; i < editHits_.size(); ++i)
  {
    const EditHit &hit = editHits_[i];
    if (hit.text && PtInRect(point, &hit.rect))
    {
      return true;
    }
  }
  return false;
}


void ToolboxScenePlatformController::beginClip(const Rect &rect)
{
  if (clipRgn_)
  {
    GetClip(clipRgn_);
    ClipRect(&rect);
    hasClip_ = true;
  }
}

void ToolboxScenePlatformController::endClip()
{
  if (clipRgn_ && hasClip_)
  {
    SetClip(clipRgn_);
    hasClip_ = false;
  }
}

bool ToolboxScenePlatformController::handleControlClick(const Point &point)
{
  if (!window_ || !window_->window())
  {
    return false;
  }
  ControlRef control = 0;
  ControlPartCode part = FindControl(point, window_->window(), &control);
  if (part == 0 || !control)
  {
    return false;
  }
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    ButtonControlBinding &binding = buttonControls_[i];
    if (binding.control == control && binding.emitter)
    {
      beginBatchUpdate();
      ControlPartCode tracked = TrackControl(control, point, 0);
      if (tracked != 0)
      {
        binding.emitter->emit();
      }
      endBatchUpdate();
      return true;
    }
  }
  return false;
}

void ToolboxScenePlatformController::TextStateChangedThunk(void *userData)
{
  TextBinding *binding = static_cast<TextBinding *>(userData);
  if (!binding || !binding->controller)
  {
    return;
  }
  binding->controller->handleTextChanged(binding->state);
}
