#include "core2/scene/ContextDefinition.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      int ContextDefinitionBase::nextId_ = 1;

      ContextDefinitionBase::ContextDefinitionBase() : id_(nextId_++) {}

      ContextDefinitionBase::ContextDefinitionBase(const ContextDefinitionBase &other) : id_(other.id_) {}

      ContextDefinitionBase &ContextDefinitionBase::operator=(const ContextDefinitionBase &other)
      {
        if (this != &other)
        {
          id_ = other.id_;
        }
        return *this;
      }

    } // namespace scene
  } // namespace core
} // namespace declara
