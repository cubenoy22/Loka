#ifndef LOKA_TOOLBOX_HIT_LEDGER_HPP
#define LOKA_TOOLBOX_HIT_LEDGER_HPP

#include "core/State.hpp"
#include "core/Vector.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include <Quickdraw.h>
#include <vector>

class ToolboxScenePlatformController;
class ToolboxButtonContext;
class ToolboxPopupMenuContext;
class ToolboxCellContext;

/** Owns the five hit registries projected by the Toolbox controller. */
class ToolboxHitLedger
{
public:
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

private:
  friend class ToolboxScenePlatformController;

  std::vector<ButtonHit> buttonHits_;
  std::vector<CellHit> cellHits_;
  std::vector<EditHit> editHits_;
  std::vector<PopupHit> popupHits_;
  std::vector<TextHit> textHits_;
};

#endif // LOKA_TOOLBOX_HIT_LEDGER_HPP
