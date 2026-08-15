#ifndef LOKA_TOOLBOX_ENABLED_CHANGE_DISPATCH_HPP
#define LOKA_TOOLBOX_ENABLED_CHANGE_DISPATCH_HPP

#include <cstddef>
#include <vector>

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

/** Owns the observer records that carry one enabled State through the Toolbox
    controller callback path and into the all-kind dispatch. Sink supplies
    applyEnabledChangeForKind; the actual controller and the host contract rig
    use this same binding, thunk, and dispatch mechanism. */
template <typename Sink, typename StateType>
class ToolboxEnabledStateBindingPath
{
public:
  explicit ToolboxEnabledStateBindingPath(Sink *sink)
      : sink_(sink),
        bindings_()
  {
  }

  ~ToolboxEnabledStateBindingPath()
  {
    this->clear();
  }

  void bind(StateType *state)
  {
    if (!state || this->find(state) != this->bindings_.size())
    {
      return;
    }
    Binding *binding = new Binding();
    if (!binding)
    {
      return;
    }
    binding->state = state;
    binding->path = this;
    this->bindings_.push_back(binding);
    state->bind(&ToolboxEnabledStateBindingPath::StateChangedThunk,
                binding,
                false,
                false,
                0);
  }

  void unbind(StateType *state)
  {
    const std::size_t index = this->find(state);
    if (index == this->bindings_.size())
    {
      return;
    }
    this->releaseAt(index);
    this->bindings_.erase(this->bindings_.begin() + index);
  }

  void clear()
  {
    for (std::size_t i = 0; i < this->bindings_.size(); ++i)
    {
      this->releaseAt(i);
    }
    this->bindings_.clear();
  }

private:
  struct Binding
  {
    StateType *state;
    ToolboxEnabledStateBindingPath *path;
  };

  std::size_t find(StateType *state) const
  {
    for (std::size_t i = 0; i < this->bindings_.size(); ++i)
    {
      if (this->bindings_[i] && this->bindings_[i]->state == state)
      {
        return i;
      }
    }
    return this->bindings_.size();
  }

  void releaseAt(std::size_t index)
  {
    Binding *binding = this->bindings_[index];
    if (!binding)
    {
      return;
    }
    if (binding->state)
    {
      binding->state->unbind(
          &ToolboxEnabledStateBindingPath::StateChangedThunk,
          binding);
    }
    binding->state = 0;
    binding->path = 0;
    delete binding;
  }

  void handleEnabledChanged(StateType *state)
  {
    if (!this->sink_ || !this->sink_->enabledChangeDispatchReady())
    {
      return;
    }
    DispatchToolboxEnabledChange(
        *this->sink_, state, &Sink::applyEnabledChangeForKind);
  }

  static void StateChangedThunk(void *userData)
  {
    Binding *binding = static_cast<Binding *>(userData);
    if (!binding || !binding->path || !binding->state)
    {
      return;
    }
    binding->path->handleEnabledChanged(binding->state);
  }

  Sink *sink_;
  std::vector<Binding *> bindings_;
};

#endif // LOKA_TOOLBOX_ENABLED_CHANGE_DISPATCH_HPP
