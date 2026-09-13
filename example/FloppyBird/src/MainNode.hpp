#ifndef LOKA_FLOPPY_BIRD_MAIN_NODE_HPP
#define LOKA_FLOPPY_BIRD_MAIN_NODE_HPP

#include "app/scene/BorrowedKeys.hpp"

#include "app/RectSurface.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
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

    MainProps() {}

    explicit MainProps(SharedModel *shared)
    {
      this->keys_.set(KEY_SHARED, shared);
    }

    MainProps &shared(SharedModel *shared)
    {
      this->keys_.set(KEY_SHARED, shared);
      return *this;
    }

    SharedModel *shared() const
    {
      return static_cast<SharedModel *>(const_cast<void *>(this->keys_.get(KEY_SHARED)));
    }

    void assertInitialized() const
    {
      assert(this->keys_.complete());
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() &&
             this->keys_ < static_cast<const MainProps &>(rhs).keys_;
    }

  private:
    enum
    {
      KEY_SHARED,
      KEY_COUNT
    };
    loka::app::scene::BorrowedKeys<KEY_COUNT> keys_;
  };

  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
  public:
    MainNode(const MainProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      this->props.assertInitialized();
      c.declare(VStack().alignHorizontal(HORIZONTAL_ALIGNMENT_LEADING)
                << Text(&this->props.shared()->scoreText_).TEST_ID("FloppyBird.Score")
                << RectSurface(&this->props.shared()->surfaceModel_)
                       .useRegionClip(false)
                       .size(loka_floppy_bird::kWindowWidth, loka_floppy_bird::kWindowHeight)
                       .TEST_ID("FloppyBird.Surface"));
    }
  };
} // namespace floppybird

#endif
