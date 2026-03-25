#ifndef LOKA_FLOPPY_BIRD_MAIN_NODE_HPP
#define LOKA_FLOPPY_BIRD_MAIN_NODE_HPP

#include "app/Cell.hpp"
#include "app/Grid.hpp"
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
        : titleText_(loka::core::String::Literal("LokaFloppyBird")),
          statusText_(loka::core::String::Literal("Press Space To Start")),
          scoreText_(loka::core::String::Literal("Score: 0")),
          cells_()
    {
    }

    loka::core::MutableState<loka::core::String> titleText_;
    loka::core::MutableState<loka::core::String> statusText_;
    loka::core::MutableState<loka::core::String> scoreText_;
    loka::core::MutableState<loka::core::String>
        cells_[loka_floppy_bird::kBoardRows * loka_floppy_bird::kBoardCols];
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
      c.declare(VStack()
                << Text(&this->props.shared_->titleText_)
                << Text(&this->props.shared_->statusText_)
                << Text(&this->props.shared_->scoreText_)
                << buildBoard());
    }

  private:
    loka::app::GridDefinition buildBoard()
    {
      loka::app::GridDefinition grid = loka::app::Grid()
                                           .rows(loka_floppy_bird::kBoardRows)
                                           .cols(loka_floppy_bird::kBoardCols);
      for (int i = 0; i < loka_floppy_bird::kBoardRows * loka_floppy_bird::kBoardCols; ++i)
      {
        grid << loka::app::Cell(&this->props.shared_->cells_[i]);
      }
      return grid;
    }
  };
}

#endif
