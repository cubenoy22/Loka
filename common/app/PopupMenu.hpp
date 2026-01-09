#ifndef LOKA_APP_POPUP_MENU_HPP
#define LOKA_APP_POPUP_MENU_HPP

#include <cstddef>
#include "core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include "core2/scene/Node.hpp"

namespace declara
{
  namespace app
  {
    class PopupMenuTypeTag
    {
    };

    class PopupMenuNode;

    class IPopupMenuProps
    {
    public:
      typedef PopupMenuTypeTag TypeTag;
      virtual ~IPopupMenuProps() {}
      virtual const loka::Vector<loka::core::String> *getItems() const = 0;
      virtual State<int> *getSelectedIndex() const = 0;
      virtual State<bool> *getEnabled() const = 0;
      virtual EmitterState *getOnChange() const = 0;
    };

    struct PopupMenuProps : public declara::core::scene::NodePropsBase<PopupMenuProps>, public IPopupMenuProps
    {
      typedef PopupMenuTypeTag TypeTag;
      typedef PopupMenuNode NodeType;
      const loka::Vector<loka::core::String> *items_;
      loka::Vector<loka::core::String> ownedItems_;
      bool ownsItems_;
      State<int> *selectedIndex_;
      State<bool> *enabled_;
      EmitterState *onChange_;
      PopupMenuProps()
          : items_(0), ownedItems_(), ownsItems_(false), selectedIndex_(0), enabled_(0), onChange_(0) {}
      PopupMenuProps(const PopupMenuProps &other)
          : items_(other.items_),
            ownedItems_(other.ownedItems_),
            ownsItems_(other.ownsItems_),
            selectedIndex_(other.selectedIndex_),
            enabled_(other.enabled_),
            onChange_(other.onChange_)
      {
        if (ownsItems_)
        {
          items_ = &ownedItems_;
        }
      }
      PopupMenuProps &operator=(const PopupMenuProps &other)
      {
        if (this == &other)
        {
          return *this;
        }
        items_ = other.items_;
        ownedItems_ = other.ownedItems_;
        ownsItems_ = other.ownsItems_;
        selectedIndex_ = other.selectedIndex_;
        enabled_ = other.enabled_;
        onChange_ = other.onChange_;
        if (ownsItems_)
        {
          items_ = &ownedItems_;
        }
        return *this;
      }

      PopupMenuProps &items(const loka::Vector<loka::core::String> *items)
      {
        this->items_ = items;
        this->ownsItems_ = false;
        return *this;
      }

      PopupMenuProps &items(const loka::Vector<loka::core::String> &items)
      {
        this->ownedItems_ = items;
        this->items_ = &this->ownedItems_;
        this->ownsItems_ = true;
        return *this;
      }

      PopupMenuProps &items(const char **items, std::size_t count)
      {
        this->ownedItems_.clear();
        this->ownedItems_.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
          this->ownedItems_.push_back(loka::core::String::Literal(items[i]));
        }
        this->items_ = &this->ownedItems_;
        this->ownsItems_ = true;
        return *this;
      }

      PopupMenuProps &selectedIndex(State<int> *index)
      {
        this->selectedIndex_ = index;
        return *this;
      }

      PopupMenuProps &enabled(State<bool> *enabled)
      {
        this->enabled_ = enabled;
        return *this;
      }

      PopupMenuProps &onChange(EmitterState *onChange)
      {
        this->onChange_ = onChange;
        return *this;
      }

      virtual const loka::Vector<loka::core::String> *getItems() const { return items_; }
      virtual State<int> *getSelectedIndex() const { return selectedIndex_; }
      virtual State<bool> *getEnabled() const { return enabled_; }
      virtual EmitterState *getOnChange() const { return onChange_; }

      int hash() const
      {
        std::size_t h = 17;
        h = h * 31 + reinterpret_cast<std::size_t>(items_);
        h = h * 31 + reinterpret_cast<std::size_t>(selectedIndex_);
        h = h * 31 + reinterpret_cast<std::size_t>(enabled_);
        h = h * 31 + reinterpret_cast<std::size_t>(onChange_);
        return static_cast<int>(h);
      }

      bool operator<(const declara::core::scene::PropsBase &rhs) const
      {
        const PopupMenuProps *other = dynamic_cast<const PopupMenuProps *>(&rhs);
        if (!other)
        {
          return false;
        }
        if (items_ != other->items_)
        {
          return items_ < other->items_;
        }
        if (selectedIndex_ != other->selectedIndex_)
        {
          return selectedIndex_ < other->selectedIndex_;
        }
        return enabled_ < other->enabled_;
      }
    };

    class PopupMenuNode : public declara::core::scene::Node
    {
    public:
      typedef PopupMenuTypeTag TypeTag;
      PopupMenuProps props;
      PopupMenuNode(const PopupMenuProps &p) : props(p) {}
      virtual declara::core::scene::NodeKind kind() const { return declara::core::scene::NODE_KIND_POPUP_MENU; }
    };

    struct PopupMenuDefinition : public declara::core::scene::NodeDefinition<PopupMenuProps, PopupMenuNode>
    {
      PopupMenuDefinition() : NodeDefinition() {}
      PopupMenuDefinition(const PopupMenuProps &p) : NodeDefinition(p) {}
      PopupMenuDefinition(const loka::Vector<loka::core::String> &items) : NodeDefinition()
      {
        this->props.items(items);
      }
      PopupMenuDefinition(const loka::Vector<loka::core::String> *items) : NodeDefinition()
      {
        this->props.items(items);
      }
      PopupMenuDefinition(const char **items, std::size_t count) : NodeDefinition()
      {
        this->props.items(items, count);
      }

      PopupMenuDefinition &items(const loka::Vector<loka::core::String> *items)
      {
        this->props.items(items);
        return *this;
      }

      PopupMenuDefinition &items(const loka::Vector<loka::core::String> &items)
      {
        this->props.items(items);
        return *this;
      }

      PopupMenuDefinition &items(const char **items, std::size_t count)
      {
        this->props.items(items, count);
        return *this;
      }

      PopupMenuDefinition &selectedIndex(State<int> *index)
      {
        this->props.selectedIndex(index);
        return *this;
      }

      PopupMenuDefinition &enabled(State<bool> *enabled)
      {
        this->props.enabled(enabled);
        return *this;
      }

      PopupMenuDefinition &onChange(EmitterState *onChange)
      {
        this->props.onChange(onChange);
        return *this;
      }

      using declara::core::scene::NodeDefinition<PopupMenuProps, PopupMenuNode>::create;
    };

    typedef PopupMenuDefinition PopupMenu;
  } // namespace app
} // namespace declara

#endif // LOKA_APP_POPUP_MENU_HPP
