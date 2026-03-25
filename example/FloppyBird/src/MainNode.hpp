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

  class UiTypeTag
  {
  };

  class GameTypeTag
  {
  };

  class MainNode;
  class UiNode;
  class GameNode;

  struct SharedModel
  {
    SharedModel()
        : titleText_(loka::core::String::Literal("LokaFloppyBird")),
          statusText_(loka::core::String::Literal("Press Space To Start")),
          scoreText_(loka::core::String::Literal("Score: 0")),
          surfaceModel_()
    {
    }

    loka::core::MutableState<loka::core::String> titleText_;
    loka::core::MutableState<loka::core::String> statusText_;
    loka::core::MutableState<loka::core::String> scoreText_;
    loka::core::MutableState<loka::app::RectSurfaceModel> surfaceModel_;
  };

  struct UiProps : public loka::app::scene::NodePropsBase<UiProps>
  {
    typedef UiNode NodeType;
    typedef UiTypeTag TypeTag;

    SharedModel *shared_;

    UiProps() : shared_(0) {}
    explicit UiProps(SharedModel *shared) : shared_(shared) {}

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const UiProps &other = static_cast<const UiProps &>(rhs);
      return this->shared_ < other.shared_;
    }
  };

  struct GameProps : public loka::app::scene::NodePropsBase<GameProps>
  {
    typedef GameNode NodeType;
    typedef GameTypeTag TypeTag;

    SharedModel *shared_;

    GameProps() : shared_(0) {}
    explicit GameProps(SharedModel *shared) : shared_(shared) {}

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
      {
        return false;
      }
      const GameProps &other = static_cast<const GameProps &>(rhs);
      return this->shared_ < other.shared_;
    }
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

  class UiNode : public loka::app::scene::StaticCompositionBoundaryNodeBase<UiProps>
  {
  public:
    UiNode(const UiProps &props)
        : loka::app::scene::StaticCompositionBoundaryNodeBase<UiProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      (void)c;
      // Temporarily disable text composition while we focus on sprite redraw costs.
    }
  };

  class GameNode : public loka::app::scene::StaticCompositionBoundaryNodeBase<GameProps>
  {
  public:
    GameNode(const GameProps &props)
        : loka::app::scene::StaticCompositionBoundaryNodeBase<GameProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(RectSurface(&this->props.shared_->surfaceModel_)
                    .useRegionClip(false)
                    .size(loka_floppy_bird::kWindowWidth,
                          loka_floppy_bird::kWindowHeight));
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
      c.declare(loka::app::scene::BoundaryDefinition<GameProps, GameNode>(GameProps(this->props.shared_)));
    }
  };
}

#endif
