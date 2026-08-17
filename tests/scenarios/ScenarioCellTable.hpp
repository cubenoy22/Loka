#ifndef LOKA_TESTS_SCENARIOS_SCENARIO_CELL_TABLE_HPP
#define LOKA_TESTS_SCENARIOS_SCENARIO_CELL_TABLE_HPP

#include <cstddef>
#include <string>

namespace loka
{
  namespace scenario_tests
  {
    /** An ordered, immutable view over one example's registered cell names.

        The table borrows a static array rather than owning storage: the cell
        order is a fact about the example, not state anyone mutates. Each
        example publishes one array and derives both its reel order and its
        registration predicate from it, so a newly registered cell joins the
        reel by being registered once instead of being listed twice. */
    class ScenarioCellTable
    {
    public:
      ScenarioCellTable(const char *const *cells, std::size_t count);

      std::size_t size() const;
      bool empty() const;
      /** Returns the cell at index, or 0 when index is out of range. */
      const char *at(std::size_t index) const;
      bool contains(const std::string &name) const;
      /** Returns the table without its first count cells. */
      ScenarioCellTable dropFirst(std::size_t count) const;

    private:
      const char *const *cells_;
      std::size_t count_;
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENARIO_CELL_TABLE_HPP
