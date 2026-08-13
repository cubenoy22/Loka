#ifndef LOKA_TESTS_TOOLBOX_OBSERVED_SCRAPBOOK_MAIN_DEFINITION_HPP
#define LOKA_TESTS_TOOLBOX_OBSERVED_SCRAPBOOK_MAIN_DEFINITION_HPP

#include "MainNode.hpp"
#include "app/scene/boundary/Boundary.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    /** Creates a Scrapbook MainNode and publishes one borrowed view while the
        App-owned Window and Scene remain alive. */
    class ObservedScrapbookMainDefinition
        : public app::scene::BoundaryDefinition<scrapbook::MainProps, scrapbook::MainNode>
    {
    public:
      typedef app::scene::BoundaryDefinition<scrapbook::MainProps, scrapbook::MainNode> Base;

      ObservedScrapbookMainDefinition(const scrapbook::MainProps &props, scrapbook::MainNode **observed)
          : Base(props),
            observed_(observed)
      {
      }

      virtual app::scene::NodeDefinitionBase *clone() const
      {
        return new ObservedScrapbookMainDefinition(*this);
      }

      virtual app::scene::Node *create() const
      {
        app::scene::Node *node = Base::create();
        if (this->observed_)
        {
          *this->observed_ = node ? static_cast<scrapbook::MainNode *>(node) : 0;
        }
        return node;
      }

    private:
      scrapbook::MainNode **observed_;
    };
  } // namespace toolbox_tests
} // namespace loka

#endif // LOKA_TESTS_TOOLBOX_OBSERVED_SCRAPBOOK_MAIN_DEFINITION_HPP
