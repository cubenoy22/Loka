#ifndef LOKA_TESTS_TOOLBOX_OBSERVED_MAIN_DEFINITION_HPP
#define LOKA_TESTS_TOOLBOX_OBSERVED_MAIN_DEFINITION_HPP

#include "app/scene/boundary/Boundary.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    /** Creates one typed main Boundary and optionally publishes a borrowed
        view while the App-owned Window and Scene remain alive. */
    template <class PropsT, class NodeT>
    class ObservedMainDefinition : public app::scene::BoundaryDefinition<PropsT, NodeT>
    {
    public:
      typedef app::scene::BoundaryDefinition<PropsT, NodeT> Base;

      ObservedMainDefinition(const PropsT &props, NodeT **observed)
          : Base(props),
            observed_(observed)
      {
      }

      virtual app::scene::NodeDefinitionBase *clone() const
      {
        return new ObservedMainDefinition(*this);
      }

      virtual app::scene::Node *create() const
      {
        app::scene::Node *node = Base::create();
        if (this->observed_)
        {
          *this->observed_ = node ? static_cast<NodeT *>(node) : 0;
        }
        return node;
      }

    private:
      NodeT **observed_;
    };
  } // namespace toolbox_tests
} // namespace loka

#endif // LOKA_TESTS_TOOLBOX_OBSERVED_MAIN_DEFINITION_HPP
