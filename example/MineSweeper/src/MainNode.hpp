#ifndef LOKA_MINESWEEPER_MAIN_NODE_HPP
#define LOKA_MINESWEEPER_MAIN_NODE_HPP

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include <cstdlib>
#include <ctime>

namespace minesweeper
{
  struct MineCellTypeTag
  {
  };

  class MineCellNode;

  /** Immutable per-game facts a cell reads from the board: the mine layout
      and adjacency never change within one game, so they travel as plain
      values. A new game is a new identity (Section key), not new props. */
  struct MineCellProps : public loka::app::scene::NodePropsBase<MineCellProps>
  {
    typedef MineCellTypeTag TypeTag;
    typedef MineCellNode NodeType;

    MineCellProps()
        : isMine(false),
          adjacentCount(0)
    {
    }

    MineCellProps(bool mine, int adjacent)
        : isMine(mine),
          adjacentCount(adjacent)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return this->propsTypeId() < rhs.propsTypeId();
      }
      const MineCellProps &other = static_cast<const MineCellProps &>(rhs);
      if (this->isMine != other.isMine)
      {
        return this->isMine < other.isMine;
      }
      return this->adjacentCount < other.adjacentCount;
    }

    bool isMine;
    int adjacentCount;
  };

  /** One cell's box. It owns the presentation resident (text) and the click
      writer; whether the cell has been revealed is a node member that lives
      and dies with the box. The board never writes into this box -- reveal
      is the cell's own doing, and a new game retires the box wholesale. */
  class MineCellNode : public loka::app::scene::ComponentNode
  {
  public:
    typedef MineCellTypeTag TypeTag;
    MineCellProps props;

    explicit MineCellNode(const MineCellProps &p)
        : loka::app::scene::ComponentNode(),
          props(p),
          revealed_(false),
          text_(),
          click_()
    {
      this->state(this->text_, loka::core::String::Literal("."));
    }

  protected:
    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      this->bindForUi(this->click_, this, &MineCellNode::handleClick);
    }

    virtual void composeChildren(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::Cell(this->text_.state()).onClick(&this->click_));
    }

  private:
    void handleClick()
    {
      if (this->revealed_)
      {
        return;
      }
      this->revealed_ = true;
      if (this->props.isMine)
      {
        this->text_.set(loka::core::String::Literal("X"));
        return;
      }
      if (this->props.adjacentCount == 0)
      {
        this->text_.set(loka::core::String::Literal(" "));
        return;
      }
      this->text_.set(loka::core::String::FromInt(this->props.adjacentCount));
    }

    bool revealed_;
    loka::app::scene::NodeState<loka::core::String> text_;
    loka::core::EmitterState click_;
  };

  typedef loka::app::scene::NodeDefinition<MineCellProps, MineCellNode> MineCell;

  class MainNode;
  typedef loka::app::scene::StdCompositionPropsFor<MainNode> MainProps;

  class MainNode : public loka::app::scene::StdCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : loka::app::scene::StdCompositionNodeFor<MainNode>(MainProps(p)),
          initialized_(false),
          bank_(0)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      if (this->initialized_)
      {
        return;
      }
      this->initialized_ = true;
      this->bindForUi(this->newGameClick_, this, &MainNode::startNewGame);
      this->resetBoard();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      Column content;
      content << Button("New Game", &this->newGameClick_);
      Grid grid;
      grid.rows(kRows).cols(kCols);
      for (int i = 0; i < kCellCount; ++i)
      {
        // One owner-scope box per cell, and the cell's presentation
        // resident now lives inside it (#274's parent-declared arrays are
        // gone). A new game swaps the key bank, so the plan retires each
        // old box -- residents included -- and materializes fresh covered
        // cells.
        Section cell(static_cast<loka::app::scene::NodeTag>(
            kCellSectionKeyBase + this->bank_ * kCellCount + i));
        cell << MineCell(MineCellProps(this->mines_[i], this->countAdjacent(i)));
        grid << cell;
      }
      content << grid;
      c.declare(content);
    }

    void startNewGame()
    {
      this->resetBoard();
      this->bank_ = 1 - this->bank_;
      this->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
    }

  protected:
    // MineSweeper is the first app whose declaration itself recomposes (the
    // key bank changes on New Game); window boundaries do not re-declare on
    // UPDATE by default. This mirrors the test-support recomposing boundary
    // and the Scene root wrapper's dirty-flag check until the scene-update
    // redesign (WR-4) promotes a shared production form. Reveal clicks stay
    // on the ordinary update path: only NODE_DIRTY_CHILD re-declares.
    virtual void declareLocalRecomposition(loka::app::scene::NodeComposition &composition)
    {
      this->composeNode(composition);
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::StdCompositionNodeFor<MainNode> BaseType;
      if (event == loka::app::scene::COMPOSE_EVENT_UPDATE &&
          (context.dirtyFlags() & loka::app::scene::NODE_DIRTY_CHILD))
      {
        this->recomposeLocalComposition(context, event,
                                        this->LOCAL_RECOMPOSE_APPLY_SNAPSHOT);
        return;
      }
      BaseType::composeWithContext(context, event);
    }

  private:
    enum
    {
      kRows = 8,
      kCols = 8,
      kCellCount = kRows * kCols,
      kMineCount = 10,
      kCellSectionKeyBase = 100
    };

    bool initialized_;
    int bank_;
    bool mines_[kCellCount];
    loka::core::EmitterState newGameClick_;

    void resetBoard()
    {
      for (int i = 0; i < kCellCount; ++i)
      {
        this->mines_[i] = false;
      }
      unsigned int seed = static_cast<unsigned int>(std::time(0));
      std::srand(seed);
      int placed = 0;
      while (placed < kMineCount)
      {
        int index = std::rand() % kCellCount;
        if (!this->mines_[index])
        {
          this->mines_[index] = true;
          ++placed;
        }
      }
    }

    int countAdjacent(int index) const
    {
      int row = index / kCols;
      int col = index % kCols;
      int count = 0;
      for (int dy = -1; dy <= 1; ++dy)
      {
        for (int dx = -1; dx <= 1; ++dx)
        {
          if (dx == 0 && dy == 0)
          {
            continue;
          }
          int nr = row + dy;
          int nc = col + dx;
          if (nr < 0 || nr >= kRows || nc < 0 || nc >= kCols)
          {
            continue;
          }
          int nindex = nr * kCols + nc;
          if (this->mines_[nindex])
          {
            ++count;
          }
        }
      }
      return count;
    }
  };
} // namespace minesweeper

#endif // LOKA_MINESWEEPER_MAIN_NODE_HPP
