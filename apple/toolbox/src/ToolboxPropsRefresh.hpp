#ifndef LOKA_TOOLBOX_PROPS_REFRESH_HPP
#define LOKA_TOOLBOX_PROPS_REFRESH_HPP

#include "core/State.hpp"
#include "core/String.hpp"
#include "core/Vector.hpp"

/** Owned literals can be projected but must never acquire a live observer. */
inline loka::core::State<loka::core::String> *ToolboxLiveTextSource(
    loka::core::State<loka::core::String> *projected, bool ownsText)
{
  return ownsText ? 0 : projected;
}

/** Compare the current projection before capture overwrites it. Shared by
    Text, Cell and EditText; owned text is still a projected source. */
inline bool ToolboxTextProjectionChanged(loka::core::State<loka::core::String> *previous,
                                         loka::core::State<loka::core::String> *next)
{
  return previous != next;
}

/** Button titles are retained values, including same-address owned literals. */
inline bool ToolboxButtonProjectionChanged(
    const loka::core::String &previousLabel, const loka::core::String &label,
    loka::core::State<bool> *previousEnabled, loka::core::State<bool> *enabled,
    loka::core::EmitterState *previousEmitter, loka::core::EmitterState *emitter)
{
  return !previousLabel.equals(label) || previousEnabled != enabled || previousEmitter != emitter;
}

/** Popup replay retains sources; event dispatch reads the captured context. */
inline bool ToolboxPopupProjectionChanged(
    const loka::Vector<loka::core::String> *previousItems, const loka::Vector<loka::core::String> *items,
    loka::core::State<int> *previousSelection, loka::core::State<int> *selection,
    loka::core::State<bool> *previousEnabled, loka::core::State<bool> *enabled)
{
  return previousItems != items || previousSelection != selection || previousEnabled != enabled;
}

/** Move the observer only after all of this context's rows have been refreshed.
    The controller owns the shared subscription and decides its final user. */
template <typename Controller>
void ReconcileToolboxTextSubscription(Controller &controller,
                                      loka::core::State<loka::core::String> *previous,
                                      loka::core::State<loka::core::String> *live)
{
  if (previous != live)
  {
    controller.bindTextState(live);
    if (previous && !controller.hasLiveBinding(previous))
      controller.unbindTextState(previous);
  }
}

#endif // LOKA_TOOLBOX_PROPS_REFRESH_HPP
