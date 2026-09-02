#ifndef LOKA_TESTS_SUPPORT_LIMITED_CLONE_PROBE_HPP
#define LOKA_TESTS_SUPPORT_LIMITED_CLONE_PROBE_HPP

#include "app/scene/Node.hpp"

namespace DefinitionCloneTestSupport
{
  class CloneProbeNode;
  struct CloneProbeTypeTag
  {
  };

  extern int g_probePropsAlive;
  extern int g_probeNodesAlive;
  extern int g_probeNodesCreated;
  extern int g_limitedCloneBudget;
  extern int g_limitedCloneCalls;

  inline bool limitedCloneBudgetAllowsClone()
  {
    ++g_limitedCloneCalls;
    if (g_limitedCloneBudget == 0)
    {
      return false;
    }
    if (g_limitedCloneBudget > 0)
    {
      --g_limitedCloneBudget;
    }
    return true;
  }

  struct CloneProbeProps : public loka::app::scene::NodePropsBase<CloneProbeProps>
  {
    typedef CloneProbeTypeTag TypeTag;
    typedef CloneProbeNode NodeType;

    CloneProbeProps()
    {
      ++g_probePropsAlive;
    }

    CloneProbeProps(const CloneProbeProps &other)
        : loka::app::scene::NodePropsBase<CloneProbeProps>(other)
    {
      ++g_probePropsAlive;
    }

    ~CloneProbeProps()
    {
      --g_probePropsAlive;
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId()
                 ? false
                 : this->propsTypeId() < rhs.propsTypeId();
    }
  };

  class CloneProbeNode : public loka::app::scene::Node
  {
  public:
    typedef CloneProbeTypeTag TypeTag;
    CloneProbeProps props;

    explicit CloneProbeNode(const CloneProbeProps &p)
        : props(p)
    {
      ++g_probeNodesAlive;
      ++g_probeNodesCreated;
    }

    virtual ~CloneProbeNode()
    {
      --g_probeNodesAlive;
    }
  };

  struct CloneProbeDefinition
      : public loka::app::scene::NodeDefinition<CloneProbeProps, CloneProbeNode>
  {
    CloneProbeDefinition()
        : loka::app::scene::NodeDefinition<CloneProbeProps, CloneProbeNode>()
    {
    }
  };

  struct LimitedCloneProbeDefinition : public CloneProbeDefinition
  {
    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      if (!limitedCloneBudgetAllowsClone())
      {
        return 0;
      }
      return new LimitedCloneProbeDefinition(*this);
    }
  };

  class LimitedClonePolicyProbeNode;
  struct LimitedClonePolicyProbeTypeTag
  {
  };

  struct LimitedClonePolicyProbeProps
      : public loka::app::scene::NodePropsBase<LimitedClonePolicyProbeProps>
  {
    typedef LimitedClonePolicyProbeTypeTag TypeTag;
    typedef LimitedClonePolicyProbeNode NodeType;

    explicit LimitedClonePolicyProbeProps(int policy)
        : policy_(policy)
    {
    }

    int policy() const
    {
      return this->policy_;
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return this->propsTypeId() < rhs.propsTypeId();
      }
      return this->policy_ <
             static_cast<const LimitedClonePolicyProbeProps &>(rhs).policy_;
    }

  private:
    int policy_;
  };

  class LimitedClonePolicyProbeNode : public loka::app::scene::Node
  {
  public:
    typedef LimitedClonePolicyProbeTypeTag TypeTag;
    LimitedClonePolicyProbeProps props;

    explicit LimitedClonePolicyProbeNode(
        const LimitedClonePolicyProbeProps &probeProps)
        : props(probeProps)
    {
    }
  };

  struct LimitedClonePolicyProbeDefinition
      : public loka::app::scene::NodeDefinition<
            LimitedClonePolicyProbeProps,
            LimitedClonePolicyProbeNode>
  {
    explicit LimitedClonePolicyProbeDefinition(int policy)
        : loka::app::scene::NodeDefinition<
              LimitedClonePolicyProbeProps,
              LimitedClonePolicyProbeNode>(
              LimitedClonePolicyProbeProps(policy))
    {
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      if (!limitedCloneBudgetAllowsClone())
      {
        return 0;
      }
      return new LimitedClonePolicyProbeDefinition(*this);
    }
  };
} // namespace DefinitionCloneTestSupport

#endif // LOKA_TESTS_SUPPORT_LIMITED_CLONE_PROBE_HPP
