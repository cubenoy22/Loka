#ifndef LOKA_TOOLBOX_ENABLED_CHANGE_DISPATCH_HPP
#define LOKA_TOOLBOX_ENABLED_CHANGE_DISPATCH_HPP

enum ToolboxEnabledControlKind
{
  TOOLBOX_ENABLED_POPUP_HIT = 0,
  TOOLBOX_ENABLED_BUTTON_CONTROL,
  TOOLBOX_ENABLED_SCROLL_BAR_CONTROL,
  TOOLBOX_ENABLED_BUTTON_HIT,
  TOOLBOX_ENABLED_CONTROL_KIND_COUNT
};

template <typename Sink, typename StateType>
void DispatchToolboxEnabledChange(
    Sink &sink,
    StateType *enabled,
    bool (Sink::*apply)(ToolboxEnabledControlKind, StateType *))
{
  for (int rawKind = 0;
       rawKind < TOOLBOX_ENABLED_CONTROL_KIND_COUNT;
       ++rawKind)
  {
    const ToolboxEnabledControlKind kind =
        static_cast<ToolboxEnabledControlKind>(rawKind);
    // A match ends only this kind's own lookup. The same State may be bound
    // to another control kind, so its result must not stop the outer fanout.
    (sink.*apply)(kind, enabled);
  }
}

#endif // LOKA_TOOLBOX_ENABLED_CHANGE_DISPATCH_HPP
