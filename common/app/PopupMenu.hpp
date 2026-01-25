#ifndef LOKA_APP_POPUP_MENU_HPP
#define LOKA_APP_POPUP_MENU_HPP

#include <cstddef>
#include "core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include "core2/scene/Node.hpp"

namespace loka
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
      virtual loka::core::State<int> *getSelectedIndex() const = 0;
      virtual loka::core::State<bool> *getEnabled() const = 0;
      virtual loka::core::EmitterState *getOnChange() const = 0;
    };

    struct PopupMenuProps : public loka::core::scene::NodePropsBase<PopupMenuProps>, public IPopupMenuProps
    {
      typedef PopupMenuTypeTag TypeTag;
      typedef PopupMenuNode NodeType;
      const loka::Vector<loka::core::String> *items_;
      loka::Vector<loka::core::String> ownedItems_;
      bool ownsItems_;
      loka::core::State<int> *selectedIndex_;
      loka::core::State<bool> *enabled_;
      loka::core::EmitterState *onChange_;
      int controlTag_;
      PopupMenuProps()
          : items_(0),
            ownedItems_(),
            ownsItems_(false),
            selectedIndex_(0),
            enabled_(0),
            onChange_(0),
            controlTag_(0) {}
      PopupMenuProps(const PopupMenuProps &other)
          : items_(other.items_),
            ownedItems_(other.ownedItems_),
            ownsItems_(other.ownsItems_),
            selectedIndex_(other.selectedIndex_),
            enabled_(other.enabled_),
            onChange_(other.onChange_),
            controlTag_(other.controlTag_)
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
        controlTag_ = other.controlTag_;
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

      PopupMenuProps &selectedIndex(loka::core::State<int> *index)
      {
        this->selectedIndex_ = index;
        return *this;
      }

      PopupMenuProps &enabled(loka::core::State<bool> *enabled)
      {
        this->enabled_ = enabled;
        return *this;
      }

      PopupMenuProps &onChange(loka::core::EmitterState *onChange)
      {
        this->onChange_ = onChange;
        return *this;
      }
      PopupMenuProps &controlTag(int tag)
      {
        this->controlTag_ = tag;
        return *this;
      }

      virtual const loka::Vector<loka::core::String> *getItems() const { return items_; }
      virtual loka::core::State<int> *getSelectedIndex() const { return selectedIndex_; }
      virtual loka::core::State<bool> *getEnabled() const { return enabled_; }
      virtual loka::core::EmitterState *getOnChange() const { return onChange_; }

      int hash() const
      {
        std::size_t h = 17;
        h = h * 31 + reinterpret_cast<std::size_t>(items_);
        h = h * 31 + reinterpret_cast<std::size_t>(selectedIndex_);
        h = h * 31 + reinterpret_cast<std::size_t>(enabled_);
        h = h * 31 + reinterpret_cast<std::size_t>(onChange_);
        h = h * 31 + static_cast<std::size_t>(controlTag_);
        return static_cast<int>(h);
      }

      bool operator<(const loka::core::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const PopupMenuProps &other = static_cast<const PopupMenuProps &>(rhs);
        if (items_ != other.items_)
          return items_ < other.items_;
        if (controlTag_ != other.controlTag_)
          return controlTag_ < other.controlTag_;
        if (selectedIndex_ != other.selectedIndex_)
          return selectedIndex_ < other.selectedIndex_;
        return enabled_ < other.enabled_;
      }
    };

    class PopupMenuNode : public loka::core::scene::Node
    {
    public:
      typedef PopupMenuTypeTag TypeTag;
      PopupMenuProps props;
      PopupMenuNode(const PopupMenuProps &p) : props(p) {}
      virtual loka::core::scene::NodeKind kind() const { return loka::core::scene::NODE_KIND_POPUP_MENU; }
      virtual PopupMenuNode *asPopupMenuNode() { return this; }
    };

    struct PopupMenuDefinition : public loka::core::scene::NodeDefinition<PopupMenuProps, PopupMenuNode>
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

      PopupMenuDefinition &selectedIndex(loka::core::State<int> *index)
      {
        this->props.selectedIndex(index);
        return *this;
      }

      PopupMenuDefinition &enabled(loka::core::State<bool> *enabled)
      {
        this->props.enabled(enabled);
        return *this;
      }

      PopupMenuDefinition &onChange(loka::core::EmitterState *onChange)
      {
        this->props.onChange(onChange);
        return *this;
      }

      PopupMenuDefinition &controlTag(int tag)
      {
        this->props.controlTag(tag);
        return *this;
      }

      using loka::core::scene::NodeDefinition<PopupMenuProps, PopupMenuNode>::create;
    };

    typedef PopupMenuDefinition PopupMenu;
  } // namespace app
} // namespace loka

#endif // LOKA_APP_POPUP_MENU_HPP
