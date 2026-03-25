#ifndef LOKA_FLOPPY_BIRD_MAIN_NODE_HPP
#define LOKA_FLOPPY_BIRD_MAIN_NODE_HPP

#include "app/RectSurface.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "GameLogic.hpp"

namespace floppybird
{
  class MainTypeTag
  {
  };

  class MainNode;

  struct SharedModel
  {
    SharedModel()
        : surfaceModel_()
    {
    }

    loka::core::MutableState<loka::app::RectSurfaceModel> surfaceModel_;
  };

  struct MainProps : public loka::app::scene::NodePropsBase<MainProps>
  {
    typedef MainNode NodeType;
    typedef MainTypeTag TypeTag;

    SharedModel *shared_;

    MainProps() : shared_(0) {}
    explicit MainProps(SharedModel *shared) : shared_(shared) {}

    MainProps &shared(SharedModel *shared)
    {
      this->shared_ = shared;
      return *this;
    }

    void assertInitialized() const
    {
      assert(this->shared_);
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      return this->shared_ < other.shared_;
    }
  };

  class MainNode : public loka::app::scene::StaticCompositionBoundaryNodeBase<MainProps>
  {
  public:
    MainNode(const MainProps &props)
        : loka::app::scene::StaticCompositionBoundaryNodeBase<MainProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      c.declare(RectSurface(&this->props.shared_->surfaceModel_)
                    .useRegionClip(false)
                    .size(loka_floppy_bird::kWindowWidth,
                          loka_floppy_bird::kWindowHeight));
    }
  };
}

#endif
