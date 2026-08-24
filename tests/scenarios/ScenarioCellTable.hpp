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
        reel by being registered once instead of being listed twice.

        Header-only on purpose. Every example's rail answers `IsXxxScenario`
        through this type, and those rails are compiled into far more targets
        than the reel is -- the Classic vehicles, the macOS and Win32 scenario
        applications, several test executables. A separate translation unit
        would have to be added to every one of those source lists, and the next
        target to compile a rail would fail to link for a reason that has
        nothing to do with what it was built for. */
    class ScenarioCellTable
    {
    public:
      ScenarioCellTable(const char *const *cells, std::size_t count)
          : cells_(cells),
            count_(cells ? count : 0)
      {
      }

      std::size_t size() const
      {
        return this->count_;
      }

      bool empty() const
      {
        return this->count_ == 0;
      }

      /** Returns the cell at index, or 0 when index is out of range. */
      const char *at(std::size_t index) const
      {
        if (index >= this->count_)
        {
          return 0;
        }
        return this->cells_[index];
      }

      bool contains(const std::string &name) const
      {
        for (std::size_t i = 0; i < this->count_; ++i)
        {
          if (this->cells_[i] && name == this->cells_[i])
          {
            return true;
          }
        }
        return false;
      }

      /** Returns the cell after name, wrapping to the first cell. An empty
          table or unknown name declines with null. */
      const char *nextAfter(const std::string &name) const
      {
        if (this->count_ == 0)
        {
          return 0;
        }
        for (std::size_t i = 0; i < this->count_; ++i)
        {
          if (this->cells_[i] && name == this->cells_[i])
          {
            return this->cells_[(i + 1) % this->count_];
          }
        }
        return 0;
      }

      /** Returns the table without its first count cells. */
      ScenarioCellTable dropFirst(std::size_t count) const
      {
        if (count >= this->count_)
        {
          return ScenarioCellTable(this->cells_, 0);
        }
        return ScenarioCellTable(this->cells_ + count, this->count_ - count);
      }

    private:
      const char *const *cells_;
      std::size_t count_;
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENARIO_CELL_TABLE_HPP
