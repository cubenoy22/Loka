#ifndef LOKA_MINESWEEPER_MAIN_NODE_HPP
#define LOKA_MINESWEEPER_MAIN_NODE_HPP

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/Grid.hpp"
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
      values. A new game would be a new identity (Section key), not new
      props -- the runtime New Game trigger is parked on the
      newgame-277-acceptance branch until the platform apply seam it needs
      is fixed (#277). */
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
      is the cell's own doing. */
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
          initialized_(false)
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
      std::srand(static_cast<unsigned int>(std::time(0)));
      this->resetBoard();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      Grid grid;
      grid.rows(kRows).cols(kCols);
      for (int i = 0; i < kCellCount; ++i)
      {
        // One owner-scope box per cell, and the cell's presentation
        // resident lives inside it (#274's parent-declared arrays are
        // gone). Removing a cell's box retires exactly that cell's state.
        Section cell(static_cast<loka::app::scene::NodeTag>(
            kCellSectionKeyBase + i));
        cell << MineCell(MineCellProps(this->mines_[i], this->countAdjacent(i)));
        grid << cell;
      }
      c.declare(grid);
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
    bool mines_[kCellCount];

    void resetBoard()
    {
      for (int i = 0; i < kCellCount; ++i)
      {
        this->mines_[i] = false;
      }
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
