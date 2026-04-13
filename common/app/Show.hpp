#ifndef LOKA_APP_SHOW_HPP
#define LOKA_APP_SHOW_HPP

#include "app/Fragment.hpp"
#include "app/scene/node/Conditional.hpp"

namespace loka
{
  namespace app
  {
    class ShowDefinition : public scene::NodeDefinitionBase
    {
    public:
      explicit ShowDefinition(loka::core::State<bool> *condition)
          : scene::NodeDefinitionBase(), condition_(condition), trueBranch_(), falseBranch_() {}
      ShowDefinition(const ShowDefinition &other)
          : scene::NodeDefinitionBase(other),
            condition_(other.condition_),
            trueBranch_(other.trueBranch_),
            falseBranch_(other.falseBranch_) {}
      ShowDefinition &operator=(const ShowDefinition &other)
      {
        if (this != &other)
        {
          scene::NodeDefinitionBase::operator=(other);
          condition_ = other.condition_;
          trueBranch_ = other.trueBranch_;
          falseBranch_ = other.falseBranch_;
        }
        return *this;
      }

      virtual scene::Node *create() const
      {
        return new scene::ConditionalNode(this->props());
      }
      virtual scene::Node *createInPlace(void *mem) const
      {
        return new (mem) scene::ConditionalNode(this->props());
      }
      virtual size_t nodeSize() const
      {
        return scene::ConditionalDefinition(this->props()).nodeSize();
      }
      virtual size_t nodeAlign() const
      {
        return scene::ConditionalDefinition(this->props()).nodeAlign();
      }
      virtual scene::NodeDefinitionBase *clone() const { return new ShowDefinition(*this); }
      virtual scene::NodeKind nodeKind() const { return scene::NODE_KIND_UNKNOWN; }
      virtual const scene::PropsBase *propsBase() const { return 0; }
      virtual bool hasEquivalentProps(const scene::NodeDefinitionBase &other) const
      {
        (void)other;
        return false;
      }
      virtual bool applyPropsToNode(scene::Node *node) const
      {
        (void)node;
        return false;
      }
      ShowDefinition &operator<<(scene::NodeDefinitionBase &child)
      {
        this->trueBranch_.addChild(&child);
        return *this;
      }
      ShowDefinition &operator<<(const scene::NodeDefinitionBase &child)
      {
        this->trueBranch_.addChild(const_cast<scene::NodeDefinitionBase *>(&child));
        return *this;
      }
      ShowDefinition &operator<<(scene::NodeDefinitionBase *ownedChild)
      {
        this->trueBranch_.addOwnedChild(ownedChild);
        return *this;
      }
      ShowDefinition &operator<<(const std::vector<scene::NodeDefinitionBase *> &container)
      {
        for (size_t i = 0; i < container.size(); ++i)
        {
          this->trueBranch_.addChild(container[i]);
        }
        return *this;
      }

      scene::ConditionalProps props() const
      {
        return scene::ConditionalProps(this->condition_, const_cast<FragmentDefinition *>(&this->trueBranch_), const_cast<FragmentDefinition *>(&this->falseBranch_));
      }

      size_t childrenCount() const
      {
        return this->trueBranch_.childrenCount();
      }

    private:
      loka::core::State<bool> *condition_;
      FragmentDefinition trueBranch_;
      FragmentDefinition falseBranch_;
    };

    inline ShowDefinition Show(loka::core::State<bool> &condition)
    {
      return ShowDefinition(&condition);
    }

    inline ShowDefinition Show(const loka::core::State<bool> &condition)
    {
      return ShowDefinition(const_cast<loka::core::State<bool> *>(&condition));
    }
  } // namespace app
} // namespace loka

#endif // LOKA_APP_SHOW_HPP
