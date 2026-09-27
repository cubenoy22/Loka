#include "app/FocusPublisher.hpp"
#include "app/scene/projection/PlatformController.hpp"

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
      namespace
      {
#ifndef NDEBUG
        bool ownsFocusState(scene::Node &root, const FocusBinding &binding)
        {
          scene::IStateOwner *owner = root.asStateOwner();
          if (owner && binding.usesTracker(owner->tracker())) return true;
          scene::INestable *children = root.asNestable();
          for (scene::Node *child = children ? children->childrenHead() : 0; child; child = child->nextInComposition)
            if (ownsFocusState(*child, binding)) return true;
          return false;
        }
#endif
      }

      scene::NodeContext *FocusPublisher::resolve(scene::SceneFocus &current, const FocusBinding &identity)
      {
        for (scene::FocusRow *row = current.head_; row; row = row->next_)
        {
          FocusParticipant *target = FocusParticipant::from(row);
          if (target && attached(*target) && target->binding_.same(identity))
            return target->context();
        }
        return 0;
      }

      void FocusPublisher::complete(scene::SceneFocus &current, scene::IPlatformController &platform, scene::Node &root)
      {
        scene::SceneFocus::Publication completion(current);
        scene::NodeContext *context = 0;
        if (!platform.readNativeFocus(context)) return;
        FocusParticipant *native = context && context->owner()
            ? FocusParticipant::from(context->owner()->asFocusParticipant()) : 0;
        const bool admitted = native && native->owner_ == &current && attached(*native);
        const FocusBinding observation = admitted ? native->binding_ : FocusBinding();
        scene::NodeContext *const identity = context;
        context = 0;
        FocusParticipant *previous = FocusParticipant::from(current.published_.peerRow());
        if (admitted && previous && previous != native && previous->binding_.sameFact(observation))
          observation.consume();
        // D1 may notify and retire the observed field. The same callback-free
        // membership walk recovers its identity without dereferencing the old context.
        FocusBinding selected;
        for (scene::FocusRow *row = current.head_; row; row = row->next_)
        {
          FocusParticipant *target = FocusParticipant::from(row);
          if (!target || !attached(*target) || !target->context() || !target->binding_.state()) continue;
#ifndef NDEBUG
          assert(ownsFocusState(root, target->binding_) && "Focus must be declared in this Scene");
#endif
          if (target->context() == identity && target->binding_.same(observation))
            context = target->context();
          if (target->binding_.requested())
          {
            selected = target->binding_;
            break;
          }
        }
        (void)root;
        if (selected.state())
        {
          selected.consume();
          scene::NodeContext *target = resolve(current, selected);
          if (target) platform.applyNativeFocus(*target);
          context = 0;
          const bool answered = platform.readNativeFocus(context);
          reconcile(current, answered, context);
        }
        else reconcile(current, true, context);
      }

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
          const FocusParticipant *other = FocusParticipant::from(row);
          assert((!other || other == &target || !attached(*other) || !target.binding_.same(other->binding_))
                 && "duplicate focus fact and key");
        }
        for (scene::SceneFocus *focus = scene::SceneFocus::registryHead(); focus; focus = focus->registryNext_)
        {
          const FocusParticipant *other = FocusParticipant::from(focus->published_.peerRow());
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

      scene::NodeContext *FocusPublisher::publishedContext(scene::SceneFocus &current)
      {
        FocusParticipant *row = FocusParticipant::from(current.published_.peerRow());
        return row && row->owner_ == &current && attached(*row) ? row->context() : 0;
      }

      void FocusPublisher::reconcile(scene::SceneFocus &current, bool answered, scene::NodeContext *context)
      {
        if (!answered)
          return;
        scene::Node *node = context ? context->owner() : 0;
        FocusParticipant *target = node ? FocusParticipant::from(node->asFocusParticipant()) : 0;
        if (target && (target->owner_ != &current || !attached(*target) || !target->binding_.state()))
          target = 0;
        FocusParticipant *previous = FocusParticipant::from(current.published_.peerRow());
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
