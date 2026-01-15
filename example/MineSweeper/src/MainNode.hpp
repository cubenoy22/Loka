#ifndef LOKA_MINESWEEPER_MAIN_NODE_HPP
#define LOKA_MINESWEEPER_MAIN_NODE_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "app/Cell.hpp"
#include "app/Grid.hpp"

namespace minesweeper
{
  class MainNode;
  typedef declara::core::scene::StaticCompositionPropsFor<MainNode> MainProps;

  class MainNode : public declara::core::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : declara::core::scene::StaticCompositionNodeFor<MainNode>(MainProps(p))
    {
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      Grid grid;
      grid.rows(8).cols(8);
      for (int i = 0; i < 64; ++i)
      {
        grid << Cell(".");
      }
      c.declare(grid);
    }
  };
} // namespace minesweeper

#endif // LOKA_MINESWEEPER_MAIN_NODE_HPP
