#ifndef LOKA_MINESWEEPER_MAIN_NODE_HPP
#define LOKA_MINESWEEPER_MAIN_NODE_HPP

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/node/ComponentNode.hpp"

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
#if defined(TEST_BUILD)
          , cellIndex(0)
#endif
    {
    }

    MineCellProps(bool mine, int adjacent, int index)
        : isMine(mine),
          adjacentCount(adjacent)
#if defined(TEST_BUILD)
          , cellIndex(index)
#endif
    {
#if !defined(TEST_BUILD)
      (void)index;
#endif
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
      if (this->adjacentCount != other.adjacentCount)
      {
        return this->adjacentCount < other.adjacentCount;
      }
#if defined(TEST_BUILD)
      return this->cellIndex < other.cellIndex;
#else
      return false;
#endif
    }

    bool isMine;
    int adjacentCount;
#if defined(TEST_BUILD)
    int cellIndex;
#endif
  };

  /** One cell's box. It owns the presentation resident (text) and the click
      writer; whether the cell has been revealed is a node member that lives
      and dies with the box. The board never writes into this box -- reveal
      is the cell's own doing, and a new game retires the box wholesale. */
  class MineCellNode : public loka::app::scene::ComponentNodeWithProps<MineCellProps>
  {
    typedef loka::app::scene::ComponentNodeWithProps<MineCellProps> Base;

  public:
    explicit MineCellNode(const MineCellProps &p)
        : Base(p),
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
#if defined(TEST_BUILD)
      c.declare(loka::app::Cell(this->text_.state()).onClick(&this->click_).TEST_ID(this->testId()));
#else
      c.declare(loka::app::Cell(this->text_.state()).onClick(&this->click_));
#endif
    }

  private:
#if defined(TEST_BUILD)
    const char *testId() const
    {
      // Keep nonempty IDs on the three scenario-actuated cells. On Classic,
      // assigning all 64 IDs would add an allocation to every test resident.
      if (this->props.cellIndex == 0)
      {
        return "MineSweeper.Cell.0";
      }
      if (this->props.cellIndex == 2)
      {
        return "MineSweeper.Cell.2";
      }
      if (this->props.cellIndex == 3)
      {
        return "MineSweeper.Cell.3";
      }
      return "";
    }
#endif

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

  struct MainTypeTag
  {
  };

  class MainNode;

  /** Completed startup input for a MineSweeper board sequence. The caller
      chooses the seed; the node owns only the generator state derived from
      this value and never consults an ambient clock or random generator. */
  struct MainProps : public loka::app::scene::NodePropsBase<MainProps>
  {
    typedef MainTypeTag TypeTag;
    typedef MainNode NodeType;

    explicit MainProps(unsigned long seed)
        : seed_(seed)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const MainProps &other = static_cast<const MainProps &>(rhs);
      return this->seed_ < other.seed_;
    }

    unsigned long seed_;
  };

  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
  public:
    typedef MainTypeTag TypeTag;

    MainNode(const MainProps &p)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(p),
          initialized_(false),
          bank_(0),
          boardRandom_(p.seed_)
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
      this->bindUi();
      this->resetBoard();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      Column content;
      content << loka::app::Button("New Game", &this->newGameClick_)
                     .TEST_ID("MineSweeper.NewGameButton");
      Grid grid;
      grid.rows(kRows).cols(kCols).TEST_ID("MineSweeper.Board");
      for (int i = 0; i < kCellCount; ++i)
      {
        // One owner-scope box per cell, and the cell's presentation
        // resident now lives inside it (#274's parent-declared arrays are
        // gone). A new game swaps the key bank, so the plan retires each
        // old box -- residents included -- and materializes fresh covered
        // cells.
        Section cell(static_cast<loka::app::scene::NodeTag>(
            kCellSectionKeyBase + this->bank_ * kCellCount + i));
        cell << scene::Component(
            MineCellProps(this->mines_[i], this->countAdjacent(i), i));
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
      typedef loka::app::scene::StdCompositionBoundaryNodeBase<MainProps> BaseType;
      if (event == loka::app::scene::COMPOSE_EVENT_UPDATE &&
          (context.dirtyFlags() & loka::app::scene::NODE_DIRTY_CHILD))
      {
        this->recomposeLocalComposition(context, event,
                                        this->LOCAL_RECOMPOSE_APPLY_SNAPSHOT);
        // beginComposition released this node's callbacks; a recomposing
        // boundary must re-declare its UI bindings or the second New Game
        // click emits into nothing (another WR-4 wrinkle to fold).
        this->bindUi();
        return;
      }
      BaseType::composeWithContext(context, event);
    }

  private:
    class BoardRandom
    {
    public:
      explicit BoardRandom(unsigned long seed)
          : state_(seed)
      {
      }

      int nextIndex(int upperBound)
      {
        // Fixed C++98 arithmetic keeps a seed's board sequence independent
        // of platform C-library rand() implementations and other app code.
        this->state_ =
            (this->state_ * 1664525UL + 1013904223UL) & 0xFFFFFFFFUL;
        return static_cast<int>((this->state_ >> 16) %
                                static_cast<unsigned long>(upperBound));
      }

    private:
      unsigned long state_;
    };

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
    BoardRandom boardRandom_;
    bool mines_[kCellCount];
    loka::core::EmitterState newGameClick_;

    void bindUi()
    {
      this->bindForUi(this->newGameClick_, this, &MainNode::startNewGame);
    }

    void resetBoard()
    {
      for (int i = 0; i < kCellCount; ++i)
      {
        this->mines_[i] = false;
      }
      int placed = 0;
      while (placed < kMineCount)
      {
        int index = this->boardRandom_.nextIndex(kCellCount);
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
