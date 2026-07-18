#ifndef LOKA_APP_SHOW_HPP
#define LOKA_APP_SHOW_HPP

#include "app/nodes/nestable/Fragment.hpp"
#include "app/scene/node/Conditional.hpp"

namespace loka
{
  namespace app
  {
    class ShowDefinition : public scene::NodeDefinitionBase
    {
    public:
      explicit ShowDefinition(loka::core::State<bool> *condition)
          : scene::NodeDefinitionBase(),
            trueBranch_(),
            falseBranch_(),
            props_(condition, &this->trueBranch_, &this->falseBranch_)
      {
      }
      ShowDefinition(const ShowDefinition &other)
          : scene::NodeDefinitionBase(other),
            trueBranch_(other.trueBranch_),
            falseBranch_(other.falseBranch_),
            props_(other.props_.condition, &this->trueBranch_, &this->falseBranch_)
      {
      }
      ShowDefinition &operator=(const ShowDefinition &other)
      {
        if (this != &other)
        {
          scene::NodeDefinitionBase::operator=(other);
          trueBranch_ = other.trueBranch_;
          falseBranch_ = other.falseBranch_;
          props_.condition = other.props_.condition;
          props_.trueDef = &this->trueBranch_;
          props_.falseDef = &this->falseBranch_;
        }
        return *this;
      }

      virtual scene::Node *create() const
      {
        scene::Node *node = new scene::ConditionalNode(this->props());
        if (node)
        {
          node->setNodeTag(this->nodeTag());
          node->setNativeLifetimeHint(this->nativeLifetimeHint());
        }
        return node;
      }
      virtual scene::Node *createInPlace(void *mem) const
      {
        scene::Node *node = new (mem) scene::ConditionalNode(this->props());
        if (node)
        {
          node->setNodeTag(this->nodeTag());
          node->setNativeLifetimeHint(this->nativeLifetimeHint());
        }
        return node;
      }
      virtual size_t nodeSize() const
      {
        return scene::ConditionalDefinition(this->props()).nodeSize();
      }
      virtual size_t nodeAlign() const
      {
        return scene::ConditionalDefinition(this->props()).nodeAlign();
      }
      virtual scene::NodeDefinitionBase *clone() const
      {
        return new ShowDefinition(*this);
      }
      virtual scene::NodeKind nodeKind() const
      {
        return scene::NODE_KIND_UNKNOWN;
      }
      virtual const scene::PropsBase *propsBase() const
      {
        return &this->props_;
      }
      virtual bool hasEquivalentProps(const scene::NodeDefinitionBase &other) const
      {
        const scene::PropsBase *otherProps = other.propsBase();
        if (!otherProps || otherProps->propsTypeId() != this->props_.propsTypeId())
        {
          return false;
        }
        const scene::ConditionalProps &otherConditionalProps =
            static_cast<const scene::ConditionalProps &>(*otherProps);
        return this->props_.condition == otherConditionalProps.condition;
      }
      virtual bool applyPropsToNode(scene::Node *node) const
      {
        if (!node || node->nodeTypeKey() != scene::NodeTypeToken<scene::ConditionalNode>())
        {
          return false;
        }
        scene::ConditionalNode *conditional = static_cast<scene::ConditionalNode *>(node);
        conditional->applyRetainedProps(this->props_);
        conditional->setNodeTag(this->nodeTag());
        conditional->setNativeLifetimeHint(this->nativeLifetimeHint());
        return true;
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

      const scene::ConditionalProps &props() const
      {
        return this->props_;
      }

      size_t childrenCount() const
      {
        return this->trueBranch_.childrenCount();
      }

    private:
      FragmentDefinition trueBranch_;
      FragmentDefinition falseBranch_;
      scene::ConditionalProps props_;
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
