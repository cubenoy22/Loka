#ifndef LOKA_DSL_SLOT_HPP
#define LOKA_DSL_SLOT_HPP

#include "loka/dsl/Expr.hpp"

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
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_SLOT_HPP
