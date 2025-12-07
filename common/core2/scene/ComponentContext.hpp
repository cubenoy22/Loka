#ifndef DECLARA_CORE2_SCENE_COMPONENTCONTEXT_HPP
#define DECLARA_CORE2_SCENE_COMPONENTCONTEXT_HPP

#include <vector>
#include <typeinfo>
#include <cassert>
#include <cstdlib>
#include "core2/scene/ContextDefinition.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class ComposableNode;

      class ComponentContext
      {
      public:
        ComponentContext() : parent_(0), owner_(0) {}
        explicit ComponentContext(ComponentContext *parent) : parent_(parent), owner_(0) {}

        ComponentContext *parent() const { return parent_; }
        void setParent(ComponentContext *parent) { parent_ = parent; }
        ComposableNode *owner() const { return owner_; }
        void setOwner(ComposableNode *owner) { owner_ = owner; }

        template <class T>
        void provide(const ContextDefinition<T> &definition, T *value)
        {
          assert(value && "ComponentContext::provide requires non-null pointer");
          Entry entry;
          entry.definitionId = definition.id();
          entry.type = &typeid(T);
          entry.value = value;
          entries_.push_back(entry);
        }

        template <class T>
        void provide(const ContextDefinition<T> &definition, T &value)
        {
          provide(definition, &value);
        }

        template <class T>
        T &require(const ContextDefinition<T> &definition) const
        {
          T *value = this->find(definition);
          if (!value)
          {
            assert(false && "ComponentContext::require: context not found");
            std::abort();
          }
          return *value;
        }

        template <class T>
        bool contains(const ContextDefinition<T> &definition) const
        {
          return this->find(definition) != 0;
        }

        template <class T>
        T *get(const ContextDefinition<T> &definition) const
        {
          return this->find(definition);
        }

        void clearLocal()
        {
          entries_.clear();
        }

      private:
        friend class ComposableNode;
        struct Entry
        {
          int definitionId;
          const std::type_info *type;
          void *value;
        };

        template <class T>
        T *find(const ContextDefinition<T> &definition) const
        {
          const Entry *entry = this->findEntry(definition.id());
          if (!entry)
          {
            return 0;
          }
          assert(*(entry->type) == typeid(T) && "ComponentContext::find: type mismatch");
          return static_cast<T *>(entry->value);
        }

        const Entry *findEntry(int definitionId) const
        {
          const ComponentContext *ctx = this;
          while (ctx)
          {
            for (size_t i = ctx->entries_.size(); i > 0; --i)
            {
              const Entry &entry = ctx->entries_[i - 1];
              if (entry.definitionId == definitionId)
              {
                return &entry;
              }
            }
            ctx = ctx->parent_;
          }
          return 0;
        }

        ComponentContext *parent_;
        ComposableNode *owner_;
        std::vector<Entry> entries_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_COMPONENTCONTEXT_HPP
