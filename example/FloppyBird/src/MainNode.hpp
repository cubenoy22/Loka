#ifndef LOKA_FLOPPY_BIRD_MAIN_NODE_HPP
#define LOKA_FLOPPY_BIRD_MAIN_NODE_HPP

#include "app/RectSurface.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
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
        : scoreText_(loka::core::String::Literal("Score: 0")),
          surfaceModel_()
    {
    }

    loka::core::MutableState<loka::core::String> scoreText_;
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
#if defined(LOKA_RETRO68)
      c.declare(VStack().alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING)
                    << RectSurface(&this->props.shared_->surfaceModel_)
                           .useRegionClip(false)
                           .size(loka_floppy_bird::kWindowWidth,
                                 loka_floppy_bird::kWindowHeight));
#else
      c.declare(VStack().alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING)
                    << Text(&this->props.shared_->scoreText_)
                    << RectSurface(&this->props.shared_->surfaceModel_)
                           .useRegionClip(false)
                           .size(loka_floppy_bird::kWindowWidth,
                                 loka_floppy_bird::kWindowHeight));
#endif
    }
  };
}

#endif
