#ifndef LOKA_TESTS_SUPPORT_FULL_REBUILD_LEDGER_DEFINITION_HPP
#define LOKA_TESTS_SUPPORT_FULL_REBUILD_LEDGER_DEFINITION_HPP

#include "app/scene/Node.hpp"

namespace SceneTestSupport
{
  /** Test facade that selects the definition cloned for the current compose pass. */
  class FullRebuildLedgerDefinition : public loka::app::scene::NodeDefinitionBase
  {
  public:
    FullRebuildLedgerDefinition(bool *useReplacement,
                                loka::app::scene::NodeDefinitionBase *initial,
                                loka::app::scene::NodeDefinitionBase *replacement)
        : useReplacement_(useReplacement),
          initial_(initial),
          replacement_(replacement)
    {
    }

    virtual loka::app::scene::Node *create() const
    {
      return this->selected()->create();
    }

    virtual loka::app::scene::Node *createInPlace(void *memory) const
    {
      return this->selected()->createInPlace(memory);
    }

    virtual size_t nodeSize() const
    {
      return this->selected()->nodeSize();
    }

    virtual size_t nodeAlign() const
    {
      return this->selected()->nodeAlign();
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return this->selected()->clone();
    }

    virtual loka::app::scene::NodeKind nodeKind() const
    {
      return this->selected()->nodeKind();
    }

    virtual const loka::app::scene::PropsBase *propsBase() const
    {
      return this->selected()->propsBase();
    }

    virtual bool hasEquivalentProps(
        const loka::app::scene::NodeDefinitionBase &other) const
    {
      return this->selected()->hasEquivalentProps(other);
    }

    virtual bool applyPropsToNode(loka::app::scene::Node *node) const
    {
      return this->selected()->applyPropsToNode(node);
    }

  private:
    loka::app::scene::NodeDefinitionBase *selected() const
    {
      return *this->useReplacement_ ? this->replacement_ : this->initial_;
    }

    bool *useReplacement_;
    loka::app::scene::NodeDefinitionBase *initial_;
    loka::app::scene::NodeDefinitionBase *replacement_;
  };
} // namespace SceneTestSupport

#endif // LOKA_TESTS_SUPPORT_FULL_REBUILD_LEDGER_DEFINITION_HPP
