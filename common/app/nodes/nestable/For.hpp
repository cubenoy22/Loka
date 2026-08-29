#ifndef LOKA_APP_NODES_NESTABLE_FOR_HPP
#define LOKA_APP_NODES_NESTABLE_FOR_HPP

#include <cassert>
#include <cstddef>
#include <new>

#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "core/Vector.hpp"
#include "dsl/flow/Expr.hpp"
#include "dsl/stream/Slot.hpp"

namespace loka
{
  namespace app
  {
    /** Default item-to-definition policy for Props values. Props without a
        NodeType deliberately make the no-factory For overload ill-formed. */
    template <class Item> struct ComponentItemFactory
    {
      typedef scene::NodeDefinition<Item, typename Item::NodeType> Result;

      Result operator()(const Item &item, std::size_t) const
      {
        return scene::Component(item);
      }
    };

    /** One-shot composition builder. It expands to owned Section definitions;
        no For definition or runtime resident enters the composition tree.

        This is a compose-scope temporary. It holds the factory by value and
        borrows the items, which must outlive the `parent << builder`
        expression that consumes it; a ForBuilder is not a value to store.
        Vector-backed builders resolve the current elements at append time, so
        growth before insertion is safe. */
    template <class Item, class Factory, class KeyExprT>
    class ForBuilder
    {
    public:
      ForBuilder(long base,
                 const Vector<Item> &items,
                 const Factory &factory,
                 const KeyExprT &keyExpr)
          : slot(1),
            base_(base),
            vectorItems_(&items),
            arrayItems_(0),
            arrayItemCount_(0),
            factory_(factory),
            keyExpr_(keyExpr)
      {
      }

      ForBuilder(long base,
                 const Item *items,
                 std::size_t itemCount,
                 const Factory &factory,
                 const KeyExprT &keyExpr)
          : slot(1),
            base_(base),
            vectorItems_(0),
            arrayItems_(items),
            arrayItemCount_(itemCount),
            factory_(factory),
            keyExpr_(keyExpr)
      {
      }

      /** Returns the same builder with an item-derived integer key offset.
          The public item slot has Stream::map's slot-1 meaning. */
      template <class KeyNodeT>
      ForBuilder<Item, Factory, dsl::Expr<int, KeyNodeT> >
      key(const dsl::Expr<int, KeyNodeT> &keyExpr) const
      {
        if (this->vectorItems_)
        {
          return ForBuilder<Item, Factory, dsl::Expr<int, KeyNodeT> >(
              this->base_, *this->vectorItems_, this->factory_, keyExpr);
        }
        return ForBuilder<Item, Factory, dsl::Expr<int, KeyNodeT> >(
            this->base_, this->arrayItems_, this->arrayItemCount_,
            this->factory_, keyExpr);
      }

      /** Builds the complete owned Section batch, then adopts it into parent.
          A refusal before commit leaves parent unchanged. */
      void appendTo(scene::INestableDefinition &parent) const
      {
        const std::size_t itemCount =
            this->vectorItems_ ? this->vectorItems_->size()
                               : this->arrayItemCount_;
        const Item *items =
            this->vectorItems_
                ? (this->vectorItems_->empty() ? 0 : &(*this->vectorItems_)[0])
                : this->arrayItems_;
        if (!this->validateKeys(items, itemCount))
        {
          return;
        }

        OwnedDefinitionBatch factoryChildren;
        for (std::size_t i = 0; i < itemCount; ++i)
        {
          scene::NodeDefinitionBase *ownedChild =
              this->factory_(items[i], i).clone();
          if (!ownedChild)
          {
            return;
          }
          factoryChildren.append(ownedChild);
        }

        OwnedDefinitionBatch sections;
        for (std::size_t i = 0; i < itemCount; ++i)
        {
          scene::NodeTag tag = scene::NODE_TAG_NONE;
          if (!this->evaluateTag(items, i, tag))
          {
            return;
          }
          BoundarySectionDefinition *section =
              new (std::nothrow) BoundarySectionDefinition(tag);
          if (!section)
          {
            return;
          }
          scene::NodeDefinitionBase *ownedChild = factoryChildren.takeHead();
          assert(ownedChild &&
                 "For factory batch must contain one child per item");
          if (!ownedChild)
          {
            delete section;
            return;
          }
          section->addOwnedChild(ownedChild);
          sections.append(section);
        }

        sections.adoptInto(parent);
      }

      /** The current item at EvalContext slot 1. */
      dsl::SlotProxyBase<Item> slot;

    private:
      class OwnedDefinitionBatch
      {
      public:
        OwnedDefinitionBatch()
            : head_(0),
              tail_(0)
        {
        }

        ~OwnedDefinitionBatch()
        {
          while (this->head_)
          {
            scene::NodeDefinitionBase *next =
                this->head_->nextInComposition;
            this->head_->nextInComposition = 0;
            delete this->head_;
            this->head_ = next;
          }
          this->tail_ = 0;
        }

        void append(scene::NodeDefinitionBase *definition)
        {
          assert(definition);
          definition->nextInComposition = 0;
          if (!this->head_)
          {
            this->head_ = definition;
          }
          else
          {
            this->tail_->nextInComposition = definition;
          }
          this->tail_ = definition;
        }

        scene::NodeDefinitionBase *takeHead()
        {
          scene::NodeDefinitionBase *definition = this->head_;
          if (!definition)
          {
            return 0;
          }
          this->head_ = definition->nextInComposition;
          definition->nextInComposition = 0;
          if (!this->head_)
          {
            this->tail_ = 0;
          }
          return definition;
        }

        void adoptInto(scene::INestableDefinition &parent)
        {
          while (scene::NodeDefinitionBase *definition = this->takeHead())
          {
            parent.addOwnedChild(definition);
          }
        }

      private:
        OwnedDefinitionBatch(const OwnedDefinitionBatch &);
        OwnedDefinitionBatch &operator=(const OwnedDefinitionBatch &);

        scene::NodeDefinitionBase *head_;
        scene::NodeDefinitionBase *tail_;
      };

      bool evaluateTag(const Item *items,
                       std::size_t index,
                       scene::NodeTag &out) const
      {
        dsl::EvalContext context;
        context.index = index;
        context.slots[1] = const_cast<Item *>(&items[index]);
        const long offset = static_cast<long>(this->keyExpr_.eval(context));

        // Check before adding: on 32-bit targets INT_MAX + 65535 would
        // overflow long even though both operands are individually valid.
        if (offset < 1L - this->base_ ||
            offset > 65535L - this->base_)
        {
          return false;
        }
        const long widenedTag = this->base_ + offset;
        if (widenedTag < 1L || widenedTag > 65535L)
        {
          return false;
        }
        out = static_cast<scene::NodeTag>(widenedTag);
        return true;
      }

      bool validateKeys(const Item *items, std::size_t itemCount) const
      {
        if (this->base_ < 1L || this->base_ > 65535L)
        {
          return false;
        }
        if (itemCount != 0 && !items)
        {
          return false;
        }

        for (std::size_t i = 0; i < itemCount; ++i)
        {
          scene::NodeTag candidate = scene::NODE_TAG_NONE;
          if (!this->evaluateTag(items, i, candidate))
          {
            return false;
          }
#ifndef NDEBUG
          // This is the same debug-only misuse wall as the completed sibling
          // Section duplicate scan. Release diffing already refuses duplicate
          // tagged siblings and falls back to a full rebuild.
          for (std::size_t prior = 0; prior < i; ++prior)
          {
            scene::NodeTag priorTag = scene::NODE_TAG_NONE;
            if (!this->evaluateTag(items, prior, priorTag))
            {
              return false;
            }
            if (candidate == priorTag)
            {
              assert(false &&
                     "For items require unique sibling value keys");
              return false;
            }
          }
#endif
        }
        return true;
      }

      long base_;
      const Vector<Item> *vectorItems_;
      const Item *arrayItems_;
      std::size_t arrayItemCount_;
      Factory factory_;
      KeyExprT keyExpr_;
    };

    /** Builds Sections for Component Props items, keyed by Index(). */
    template <class Item>
    inline ForBuilder<Item,
                      ComponentItemFactory<Item>,
                      dsl::Expr<int, dsl::IndexExpr> >
    For(long base, const Vector<Item> &items)
    {
      return ForBuilder<Item,
                        ComponentItemFactory<Item>,
                        dsl::Expr<int, dsl::IndexExpr> >(
          base, items, ComponentItemFactory<Item>(), dsl::Index());
    }

    /** Builds Sections from Vector items with a caller-supplied factory. */
    template <class Item, class Factory>
    inline ForBuilder<Item, Factory, dsl::Expr<int, dsl::IndexExpr> >
    For(long base,
        const Vector<Item> &items,
        const Factory &factory)
    {
      return ForBuilder<Item, Factory, dsl::Expr<int, dsl::IndexExpr> >(
          base, items, factory, dsl::Index());
    }

    /** Fixed-array form for allocation-free item models in compose code. */
    template <class Item, std::size_t Count, class Factory>
    inline ForBuilder<Item, Factory, dsl::Expr<int, dsl::IndexExpr> >
    For(long base,
        const Item (&items)[Count],
        const Factory &factory)
    {
      return ForBuilder<Item, Factory, dsl::Expr<int, dsl::IndexExpr> >(
          base, items, Count, factory, dsl::Index());
    }
  } // namespace app
} // namespace loka

#endif // LOKA_APP_NODES_NESTABLE_FOR_HPP
