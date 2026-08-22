#ifndef LOKA_TOOLBOX_POPUP_SELECTION_INPUT_HPP
#define LOKA_TOOLBOX_POPUP_SELECTION_INPUT_HPP

#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"

inline void PublishToolboxPopupSelection(
    loka::core::StateTracker *tracker,
    loka::core::MutableState<int> &selectedIndex,
    loka::core::EmitterState *onChange,
    int newIndex)
{
  loka::core::StateTrackerGuard guard(tracker);
  selectedIndex.set(newIndex, true);
  if (onChange)
  {
    onChange->emit();
  }
}

#endif // LOKA_TOOLBOX_POPUP_SELECTION_INPUT_HPP
