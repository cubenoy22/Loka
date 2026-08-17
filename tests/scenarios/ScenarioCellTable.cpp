#include "ScenarioCellTable.hpp"

namespace loka
{
  namespace scenario_tests
  {
    ScenarioCellTable::ScenarioCellTable(const char *const *cells, std::size_t count)
        : cells_(cells),
          count_(cells ? count : 0)
    {
    }

    std::size_t ScenarioCellTable::size() const
    {
      return this->count_;
    }

    bool ScenarioCellTable::empty() const
    {
      return this->count_ == 0;
    }

    const char *ScenarioCellTable::at(std::size_t index) const
    {
      if (index >= this->count_)
      {
        return 0;
      }
      return this->cells_[index];
    }

    bool ScenarioCellTable::contains(const std::string &name) const
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

    ScenarioCellTable ScenarioCellTable::dropFirst(std::size_t count) const
    {
      if (count >= this->count_)
      {
        return ScenarioCellTable(this->cells_, 0);
      }
      return ScenarioCellTable(this->cells_ + count, this->count_ - count);
    }
  } // namespace scenario_tests
} // namespace loka
