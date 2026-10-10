#ifndef LOKA_APP_COMPONENT_GROUP_HPP
#define LOKA_APP_COMPONENT_GROUP_HPP
#include "app/core/AppComponent.hpp"
#include "core/ObservableList.hpp"
#include <cstddef>
#include <vector>

class AppComponentGroup
{
private:
  struct Row
  {
    AppComponent *component;
    loka::core::ItemId key;

    Row(AppComponent *c, loka::core::ItemId k)
        : component(c),
          key(k)
    {
    }
  };

public:
  /** Read-only live view of component membership; must not outlive its group.
      Group mutations change the view. Index access does not transfer ownership. */
  class Components
  {
  public:
    std::size_t size() const
    {
      return this->rows_.size();
    }
    AppComponent *operator[](std::size_t index) const
    {
      return this->rows_[index].component;
    }

  private:
    friend class AppComponentGroup;
    explicit Components(const std::vector<Row> &rows)
        : rows_(rows)
    {
    }
    const std::vector<Row> &rows_;
  };

  explicit AppComponentGroup(std::vector<AppComponent *> &src)
  {
    this->takeComponents(src);
  }
  explicit AppComponentGroup(std::vector<AppComponent *> src)
  {
    this->takeComponents(src);
  }
  virtual ~AppComponentGroup()
  {
    for (std::size_t i = 0; i < this->rows_.size(); ++i)
      delete this->rows_[i].component;
  }
  Components getComponents() const
  {
    return Components(this->rows_);
  }

  /** Appends an unkeyed component and transfers its ownership to this group. */
  void adopt(AppComponent *component)
  {
    this->adopt(component, loka::core::ItemId());
  }

  /** Appends a keyed component and transfers its ownership to this group. */
  void adopt(AppComponent *component, loka::core::ItemId key)
  {
    this->rows_.push_back(Row(component, key));
  }

  /** Returns the row's key, or none when the component is absent or unkeyed. */
  loka::core::ItemId keyOf(const AppComponent *component) const
  {
    for (std::size_t i = 0; i < this->rows_.size(); ++i)
      if (this->rows_[i].component == component)
        return this->rows_[i].key;
    return loka::core::ItemId();
  }

  /** Returns the keyed component, or null for none/absent; walks this group's rows. */
  AppComponent *find(loka::core::ItemId key) const
  {
    if (key.isNone())
      return 0;
    for (std::size_t i = 0; i < this->rows_.size(); ++i)
      if (this->rows_[i].key == key)
        return this->rows_[i].component;
    return 0;
  }

  /** Reserves additional rows without changing membership. Like std::vector,
      allocation failure is not recoverable here with exceptions disabled;
      Classic's global new reports out-of-memory and exits to the Finder. */
  void reserve(std::size_t additional)
  {
    this->rows_.reserve(this->rows_.size() + additional);
  }

  /** Removes without deleting; ownership returns to the caller and the result reports presence. */
  bool remove(AppComponent *component)
  {
    for (std::vector<Row>::iterator it = this->rows_.begin(); it != this->rows_.end(); ++it)
    {
      if (it->component == component)
      {
        this->rows_.erase(it);
        return true;
      }
    }
    return false;
  }

  /** Releases all components in order to the caller; the group becomes empty. */
  std::vector<AppComponent *> build()
  {
    std::vector<AppComponent *> tmp;
    tmp.reserve(this->rows_.size());
    for (std::size_t i = 0; i < this->rows_.size(); ++i)
      tmp.push_back(this->rows_[i].component);
    this->rows_.clear();
    return tmp;
  }

private:
  std::vector<Row> rows_;

  void takeComponents(std::vector<AppComponent *> &src)
  {
    this->reserve(src.size());
    for (std::size_t i = 0; i < src.size(); ++i)
      this->adopt(src[i]);
    src.clear();
  }

  AppComponentGroup(const AppComponentGroup &);
  AppComponentGroup &operator=(const AppComponentGroup &);
};

#endif // LOKA_APP_COMPONENT_GROUP_HPP
