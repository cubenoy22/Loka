#include "app/FocusPublisher.hpp"

namespace loka
{
  namespace app
  {
    void FocusParticipant::leaveAttached()
    {
      detail::FocusPublisher::leave(*this);
    }

    void FocusParticipant::rebind(const FocusBinding &previous)
    {
      if (!previous.same(this->binding_))
        detail::FocusPublisher::rebind(*this, previous);
    }

    namespace detail
    {
      bool FocusPublisher::attached(const FocusParticipant &row)
      {
        return row.node_.lifecycleFact() == scene::NODE_FACT_ATTACHED;
      }

      void FocusPublisher::audit(scene::SceneFocus &current, const FocusParticipant &target)
      {
#ifndef NDEBUG
        if (!target.binding_.state())
          return;
        for (scene::FocusRow *row = current.head_; row; row = row->next_)
        {
          const FocusParticipant &other = *static_cast<FocusParticipant *>(row);
          assert((&other == &target || !attached(other) || !target.binding_.same(other.binding_))
                 && "duplicate focus fact and key");
        }
        for (scene::SceneFocus *focus = scene::SceneFocus::registryHead(); focus; focus = focus->registryNext_)
        {
          const FocusParticipant *other = static_cast<FocusParticipant *>(focus->published_.peerRow());
          assert((focus == &current || !other || !target.binding_.sameFact(other->binding_))
                 && "focus fact published across windows");
        }
#else
        (void)current;
        (void)target;
#endif
      }

      void FocusPublisher::exchange(scene::SceneFocus &current, FocusParticipant *target, const FocusBinding &previous)
      {
        scene::SceneFocus::Publication publication(current);
        current.published_.cut();
        if (target)
          current.published_.connect(target->publication_);
        const FocusBinding next = target ? target->binding_ : FocusBinding();
        if (!previous.sameFact(next))
          previous.clear();
        // A none observer can retire the target or replace its binding. Check
        // the living edge before dereferencing, then read the current binding.
        if (target && current.published_.peerRow() == target && attached(*target))
        {
          const FocusBinding binding = target->binding_;
          binding.publish();
        }
      }

      void FocusPublisher::reconcile(scene::SceneFocus &current, bool answered, scene::NodeContext *context)
      {
        if (!answered)
          return;
        scene::Node *node = context ? context->owner() : 0;
        FocusParticipant *target = node ? static_cast<FocusParticipant *>(node->asFocusParticipant()) : 0;
        if (target && (target->owner_ != &current || !attached(*target) || !target->binding_.state()))
          target = 0;
        FocusParticipant *previous = static_cast<FocusParticipant *>(current.published_.peerRow());
        if (previous == target)
          return;
        if (target)
          audit(current, *target);
        const FocusBinding binding = previous ? previous->binding_ : FocusBinding();
        exchange(current, target, binding);
      }

      void FocusPublisher::leave(FocusParticipant &row)
      {
        scene::SceneFocus *owner = row.owner_;
        if (!owner || owner->published_.peerRow() != &row)
          return;
        const FocusBinding previous = row.binding_;
        exchange(*owner, 0, previous);
      }

      void FocusPublisher::rebind(FocusParticipant &row, const FocusBinding &previous)
      {
        scene::SceneFocus *owner = row.owner_;
        if (!owner || owner->published_.peerRow() != &row)
          return;
        audit(*owner, row);
        exchange(*owner, &row, previous);
      }
    } // namespace detail
  } // namespace app
} // namespace loka
