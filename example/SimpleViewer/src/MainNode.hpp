#ifndef LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
#define LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "app/Empty.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/ZStack.hpp"

namespace simpleviewer
{
  class MainTypeTag
  {
  };

  class MainNode;

  struct MainProps : public declara::core::scene::NodePropsBase<MainProps>
  {
    typedef MainTypeTag TypeTag;
    typedef MainNode NodeType;
    State<bool> *dialogVisible_;
    MainProps() : dialogVisible_(0) {}

    MainProps &dialogVisible(State<bool> *state)
    {
      this->dialogVisible_ = state;
      return *this;
    }

    bool operator<(const declara::core::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      return dialogVisible_ < other.dialogVisible_;
    }
  };

  class MainNode : public declara::core::scene::StaticCompositionBoundaryNodeBase<MainProps>
  {
  public:
    typedef MainTypeTag TypeTag;
    MainNode(const MainProps &p)
        : declara::core::scene::StaticCompositionBoundaryNodeBase<MainProps>(p) {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      ZStack &root = c.declare(ZStack());
      declara::core::scene::NodeComposition::ParentScope scope(c, root);
      c.declare(Empty());
      if (this->props.dialogVisible_)
      {
        c.declare(OpenFileDialog().isVisible(this->props.dialogVisible_));
      }
      else
      {
        c.declare(OpenFileDialog());
      }
    }
  };
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_MAIN_NODE_HPP
