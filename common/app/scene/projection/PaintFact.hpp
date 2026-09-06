#ifndef LOKA_PAINT_FACT_HPP
#define LOKA_PAINT_FACT_HPP
#include <cassert>
#include "app/scene/projection/PaintAnswer.hpp"
namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Completed presentation value, owned by its drawer. Invalidation never presents. */
      template <class Value> class PaintFact
      {
      public:
        PaintFact()
            : known_(false),
              value_(),
              scope_()
        {
        }
        bool isKnown() const
        {
          return this->known_;
        }
        const Value &value() const
        {
          assert(this->known_);
          return this->value_;
        }
        const PaintScope &scope() const
        {
          assert(this->known_);
          return this->scope_;
        }
        void commit(const Value &presented, const PaintScope &scope)
        {
          this->value_ = presented;
          this->scope_ = scope;
          this->known_ = true;
        }
        void invalidate()
        {
          this->known_ = false;
        }

      private:
        bool known_;
        Value value_;
        PaintScope scope_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
