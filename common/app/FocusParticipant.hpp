#ifndef LOKA_APP_FOCUS_PARTICIPANT_HPP
#define LOKA_APP_FOCUS_PARTICIPANT_HPP

#include "app/FocusBinding.hpp"
#include "app/scene/SceneFocus.hpp"
#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    /** App leaf row. The node owns this after its placement-stable props; both
        references remain valid through living detach. Reclamation is silent.
        App leaves that expose asFocusParticipant() return this row type;
        bare FocusRow is only the kernel membership/edge mechanism. */
    class FocusParticipant : public scene::FocusRow
    {
    public:
      FocusParticipant(scene::Node &node, const FocusBinding &binding)
          : node_(node),
            binding_(binding)
      {
      }
      void rebind(const FocusBinding &previous);
      /** Native read-source edge; independent of membership and publication. */
      void connectSource(scene::FocusLink &source)
      {
        source.connect(this->source_);
      }
      scene::NodeContext *context() const
      {
        return this->node_.getContext();
      }

    protected:
      virtual void leaveAttached();

    private:
      scene::Node &node_;
      const FocusBinding &binding_;
      friend class detail::FocusPublisher;
    };
  } // namespace app
} // namespace loka
#endif
