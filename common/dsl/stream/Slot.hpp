#ifndef LOKA_DSL_SLOT_HPP
#define LOKA_DSL_SLOT_HPP

#include "dsl/flow/Expr.hpp"

namespace loka
{
  namespace dsl
  {
    template <typename T>
    struct SlotProxyBase
    {
      int slotIndex;

      SlotProxyBase(int index = 1) : slotIndex(index) {}

      template <typename M, M T::*Ptr>
      Expr<M, MemberExpr<T, M, Ptr> > member() const
      {
        return Member<T, M, Ptr>(slotIndex);
      }
    };

    template <typename T>
    struct ValueSlot
    {
      int slotIndex;

      explicit ValueSlot(int index = 1) : slotIndex(index) {}

      Expr<T, ValueExpr<T> > value() const
      {
        return Value<T>(slotIndex);
      }

      operator Expr<T, ValueExpr<T> >() const
      {
        return value();
      }
    };
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_SLOT_HPP
