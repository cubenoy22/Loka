#include "ToolboxHitLedger.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"

bool ToolboxScenePlatformController::handleMouseDown(const Point &point)
{
  if (handleControlClick(point))
  {
    return false;
  }
  EditTextControlBinding *focusedEdit = editControls_.focused();
  if (focusedEdit && focusedEdit->te)
  {
    TEDeactivate(focusedEdit->te);
    editControls_.clearFocus();
  }
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    EditTextControlBinding &binding = editControls_[i];
    if (binding.te && PtInRect(point, &binding.rect))
    {
      editControls_.focus(i);
      TEActivate(binding.te);
      TEClick(point, false, binding.te);
      return true;
    }
  }
  for (size_t i = 0; i < hitLedger_.editHits_.size(); ++i)
  {
    EditHit &hit = hitLedger_.editHits_[i];
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
  for (size_t i = 0; i < hitLedger_.popupHits_.size(); ++i)
  {
    PopupHit &hit = hitLedger_.popupHits_[i];
    if (hit.context && PtInRect(point, &hit.rect) &&
        hit.context->handleMouseDown(point, this))
    {
      return false;
    }
  }
  for (size_t i = 0; i < hitLedger_.cellHits_.size(); ++i)
  {
    CellHit &hit = hitLedger_.cellHits_[i];
    if (hit.context && PtInRect(point, &hit.rect) &&
        hit.context->handleMouseDown(point, this))
    {
      return false;
    }
  }
  for (size_t i = 0; i < hitLedger_.buttonHits_.size(); ++i)
  {
    ButtonHit &hit = hitLedger_.buttonHits_[i];
    if (hit.context && PtInRect(point, &hit.rect) &&
        hit.context->handleMouseDown(point, this))
    {
      return false;
    }
  }
  return false;
}

void ToolboxScenePlatformController::recordButtonHit(const Rect &rect,
                                                     loka::core::EmitterState *emitter,
                                                     loka::core::State<bool> *enabled,
                                                     loka::app::scene::BoundaryNode *boundary,
                                                     ToolboxButtonContext *context)
{
  Rect clipped;
  if (!emitter || !this->intersectWithProjectionClip(rect, clipped))
  {
    return;
  }
  ButtonHit hit;
  hit.rect = clipped;
  hit.emitter = emitter;
  hit.enabled = enabled;
  hit.boundary = boundary;
  hit.context = context;
  hitLedger_.buttonHits_.push_back(hit);
  bindEnabledState(enabled);
}

void ToolboxScenePlatformController::recordCellHit(const Rect &rect,
                                                   loka::core::EmitterState *emitter,
                                                   loka::app::scene::BoundaryNode *boundary,
                                                   ToolboxCellContext *context,
                                                   loka::core::State<loka::core::String> *text)
{
  Rect clipped;
  if (!this->intersectWithProjectionClip(rect, clipped))
  {
    return;
  }
  CellHit hit;
  hit.rect = clipped;
  hit.emitter = emitter;
  hit.boundary = boundary;
  hit.context = context;
  hit.text = text;
  hitLedger_.cellHits_.push_back(hit);
  bindTextState(text);
}

void ToolboxScenePlatformController::recordEditHit(const Rect &rect,
                                                   loka::core::State<loka::core::String> *text,
                                                   loka::app::scene::BoundaryNode *boundary)
{
  Rect clipped;
  if (!this->intersectWithProjectionClip(rect, clipped))
  {
    return;
  }
  EditHit hit;
  hit.rect = clipped;
  hit.text = text;
  hit.boundary = boundary;
  hitLedger_.editHits_.push_back(hit);
  bindTextState(text);
  if (text && focusedText_ == text)
  {
    focusedRect_ = clipped;
    hasFocusedRect_ = true;
  }
}

void ToolboxScenePlatformController::recordTextHit(const Rect &rect,
                                                   short x,
                                                   short y,
                                                   loka::core::State<loka::core::String> *text,
                                                   loka::app::scene::BoundaryNode *boundary,
                                                   bool needsRelayoutOnChange,
                                                   short visibleWidth)
{
  Rect clipped;
  if (!text || !this->intersectWithProjectionClip(rect, clipped))
  {
    return;
  }
  TextHit hit;
  hit.rect = clipped;
  hit.x = x;
  hit.y = y;
  hit.text = text;
  hit.boundary = boundary;
  hit.lastMeasuredWidth = visibleWidth;
  hit.needsRelayoutOnChange = needsRelayoutOnChange;
  hitLedger_.textHits_.push_back(hit);
  bindTextState(text);
}

void ToolboxScenePlatformController::recordPopupHit(const Rect &rect,
                                                    short lineHeight,
                                                    const loka::Vector<loka::core::String> *items,
                                                    loka::core::State<int> *selectedIndex,
                                                    loka::core::EmitterState *onChange,
                                                    loka::core::State<bool> *enabled,
                                                    loka::app::scene::BoundaryNode *boundary,
                                                    short menuId,
                                                    ToolboxPopupMenuContext *context)
{
  Rect clipped;
  if (!this->intersectWithProjectionClip(rect, clipped))
  {
    return;
  }
  PopupHit hit;
  hit.rect = clipped;
  hit.lineHeight = lineHeight;
  hit.items = items;
  hit.selectedIndex = selectedIndex;
  hit.onChange = onChange;
  hit.enabled = enabled;
  hit.boundary = boundary;
  hit.menuId = menuId;
  hit.context = context;
  hitLedger_.popupHits_.push_back(hit);
  bindEnabledState(enabled);
}
