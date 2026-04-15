#ifndef LOKA_MINESWEEPER_MAIN_NODE_HPP
#define LOKA_MINESWEEPER_MAIN_NODE_HPP

#include "app/scene/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include <cstdlib>
#include <ctime>

namespace minesweeper
{
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
      if (this->initialized_)
      {
        return;
      }
      this->initialized_ = true;
      {
        loka::app::scene::NodeComposition::StateBatch states = c.declareStates();
        for (int i = 0; i < kCellCount; ++i)
        {
          states.state(this->cellText_[i], loka::core::String::Literal("."));
        }
      }
      for (int i = 0; i < kCellCount; ++i)
      {
        this->clickProxy_[i].owner = this;
        this->clickProxy_[i].index = i;
        this->bindForUi(this->cellClick_[i], &this->clickProxy_[i], &CellClickProxy::handleClick);
      }
      this->resetBoard();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      Grid grid;
      grid.rows(kRows).cols(kCols);
      for (int i = 0; i < kCellCount; ++i)
      {
        grid << Cell(this->cellText_[i].state()).onClick(&this->cellClick_[i]);
      }
      c.declare(grid);
    }

  private:
    enum
    {
      kRows = 8,
      kCols = 8,
      kCellCount = kRows * kCols,
      kMineCount = 10
    };

    struct CellClickProxy
    {
      CellClickProxy() : owner(0), index(0) {}
      void handleClick()
      {
        if (owner)
        {
          owner->revealCell(index);
        }
      }
      MainNode *owner;
      int index;
    };

    bool initialized_;
    bool mines_[kCellCount];
    bool revealed_[kCellCount];
    loka::core::EmitterState cellClick_[kCellCount];
    loka::app::scene::BoundState<loka::core::String> cellText_[kCellCount];
    CellClickProxy clickProxy_[kCellCount];

    void resetBoard()
    {
      for (int i = 0; i < kCellCount; ++i)
      {
        this->mines_[i] = false;
        this->revealed_[i] = false;
        this->cellText_[i].set(loka::core::String::Literal("."));
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

    void revealCell(int index)
    {
      if (index < 0 || index >= kCellCount)
      {
        return;
      }
      if (this->revealed_[index])
      {
        return;
      }
      this->revealed_[index] = true;
      if (this->mines_[index])
      {
        this->cellText_[index].set(loka::core::String::Literal("X"));
        return;
      }
      int count = countAdjacent(index);
      if (count == 0)
      {
        this->cellText_[index].set(loka::core::String::Literal(" "));
        return;
      }
      this->cellText_[index].set(loka::core::String::FromInt(count));
    }
  };
} // namespace minesweeper

#endif // LOKA_MINESWEEPER_MAIN_NODE_HPP
