#ifndef LOKA_DSL_STREAM_HPP
#define LOKA_DSL_STREAM_HPP

#include <algorithm>

#include "loka/core/Vector.hpp"
#include "dsl/flow/Expr.hpp"
#include "dsl/stream/Slot.hpp"

namespace loka
{
  namespace dsl
  {
    template <typename T> class Stream
    {
    public:
      explicit Stream(loka::Vector<T> &source)
          : source_(source),
            slot(1),
            slot2(2)
      {
      }

      template <typename ExprT> struct Comparator
      {
        const Expr<bool, ExprT> *expr_;
        explicit Comparator(const Expr<bool, ExprT> &expr)
            : expr_(&expr)
        {
        }
        bool operator()(const T &a, const T &b) const
        {
          EvalContext ctx;
          ctx.slots[1] = const_cast<T *>(&a);
          ctx.slots[2] = const_cast<T *>(&b);
          return expr_->eval(ctx);
        }
      };

      template <typename R, typename ExprT> loka::Vector<R> map(const Expr<R, ExprT> &expr) const
      {
        loka::Vector<R> result;
        result.reserve(source_.size());

        for (std::size_t i = 0; i < source_.size(); ++i)
        {
          EvalContext ctx;
          ctx.index = i;
          ctx.slots[1] = &source_[i];
          result.push_back(expr.eval(ctx));
        }

        return result;
      }

      template <typename ExprT> loka::Vector<T> filter(const Expr<bool, ExprT> &expr) const
      {
        loka::Vector<T> result;

        for (std::size_t i = 0; i < source_.size(); ++i)
        {
          EvalContext ctx;
          ctx.index = i;
          ctx.slots[1] = &source_[i];
          if (expr.eval(ctx))
          {
            result.push_back(source_[i]);
          }
        }

        return result;
      }

      template <typename ExprT> void sort(const Expr<bool, ExprT> &expr)
      {
        Comparator<ExprT> comp(expr);
        std::sort(source_.begin(), source_.end(), comp);
      }

      typename T::Slot slot;
      typename T::Slot slot2;

    private:
      loka::Vector<T> &source_;
    };
  } // namespace dsl

  template <typename T> inline dsl::Stream<T> Vector<T>::stream()
  {
    return dsl::Stream<T>(*this);
  }
} // namespace loka

#endif // LOKA_DSL_STREAM_HPP
