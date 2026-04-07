#ifndef LOKA_DSL_EXPR_HPP
#define LOKA_DSL_EXPR_HPP

#include <cstddef>

#include "loka/core/String.hpp"

namespace loka
{
  namespace dsl
  {
    typedef loka::core::String String;

    struct EvalContext
    {
      enum
      {
        MaxSlots = 4
      };

      void *slots[MaxSlots];
      std::size_t index;

      EvalContext() : slots(), index(0)
      {
        for (int i = 0; i < MaxSlots; ++i)
        {
          slots[i] = 0;
        }
      }
    };

    template <typename T, typename NodeT>
    struct Expr
    {
      typedef T Result;
      NodeT node;

      Expr() : node() {}
      explicit Expr(const NodeT &n) : node(n) {}

      T eval(const EvalContext &ctx) const { return node.eval(ctx); }
    };

    struct IndexExpr
    {
      int eval(const EvalContext &ctx) const
      {
        return static_cast<int>(ctx.index);
      }
    };

    inline Expr<int, IndexExpr> Index()
    {
      return Expr<int, IndexExpr>(IndexExpr());
    }

    struct ConstIntExpr
    {
      int value;
      ConstIntExpr() : value(0) {}
      explicit ConstIntExpr(int v) : value(v) {}
      int eval(const EvalContext &) const { return value; }
    };

    struct ConstStrExpr
    {
      const char *value;
      ConstStrExpr() : value(0) {}
      explicit ConstStrExpr(const char *v) : value(v) {}
      String eval(const EvalContext &) const
      {
        return String::Literal(value);
      }
    };

    inline Expr<int, ConstIntExpr> Const(int value)
    {
      return Expr<int, ConstIntExpr>(ConstIntExpr(value));
    }

    inline Expr<String, ConstStrExpr> Const(const char *value)
    {
      return Expr<String, ConstStrExpr>(ConstStrExpr(value));
    }

    template <typename T, typename M, M T::*Ptr>
    struct MemberExpr
    {
      int slotIndex;
      MemberExpr() : slotIndex(0) {}
      explicit MemberExpr(int index) : slotIndex(index) {}

      M eval(const EvalContext &ctx) const
      {
        T *obj = static_cast<T *>(ctx.slots[slotIndex]);
        return obj->*Ptr;
      }
    };

    template <typename T, typename M, M T::*Ptr>
    inline Expr<M, MemberExpr<T, M, Ptr> > Member(int slotIndex)
    {
      return Expr<M, MemberExpr<T, M, Ptr> >(MemberExpr<T, M, Ptr>(slotIndex));
    }

    template <typename T>
    struct ValueExpr
    {
      int slotIndex;
      ValueExpr() : slotIndex(0) {}
      explicit ValueExpr(int index) : slotIndex(index) {}

      T eval(const EvalContext &ctx) const
      {
        T *value = static_cast<T *>(ctx.slots[slotIndex]);
        return value ? *value : T();
      }
    };

    template <typename T>
    inline Expr<T, ValueExpr<T> > Value(int slotIndex)
    {
      return Expr<T, ValueExpr<T> >(ValueExpr<T>(slotIndex));
    }

    template <typename Op, typename LExpr, typename RExpr>
    struct BinaryExpr
    {
      LExpr left;
      RExpr right;

      BinaryExpr() : left(), right() {}
      BinaryExpr(const LExpr &l, const RExpr &r) : left(l), right(r) {}

      typename Op::Result eval(const EvalContext &ctx) const
      {
        return Op::apply(left.eval(ctx), right.eval(ctx));
      }
    };

    template <typename Op, typename ExprT>
    struct UnaryExpr
    {
      ExprT inner;
      UnaryExpr() : inner() {}
      explicit UnaryExpr(const ExprT &expr) : inner(expr) {}

      typename Op::Result eval(const EvalContext &ctx) const
      {
        return Op::apply(inner.eval(ctx));
      }
    };

    struct AddIntOp
    {
      typedef int Result;
      static int apply(int a, int b) { return a + b; }
    };

    struct SubIntOp
    {
      typedef int Result;
      static int apply(int a, int b) { return a - b; }
    };

    struct MulIntOp
    {
      typedef int Result;
      static int apply(int a, int b) { return a * b; }
    };

    struct DivIntOp
    {
      typedef int Result;
      static int apply(int a, int b) { return a / b; }
    };

    struct LessIntOp
    {
      typedef bool Result;
      static bool apply(int a, int b) { return a < b; }
    };

    struct GreaterIntOp
    {
      typedef bool Result;
      static bool apply(int a, int b) { return a > b; }
    };

    struct LessEqualIntOp
    {
      typedef bool Result;
      static bool apply(int a, int b) { return a <= b; }
    };

    struct GreaterEqualIntOp
    {
      typedef bool Result;
      static bool apply(int a, int b) { return a >= b; }
    };

    struct EqualIntOp
    {
      typedef bool Result;
      static bool apply(int a, int b) { return a == b; }
    };

    struct NotEqualIntOp
    {
      typedef bool Result;
      static bool apply(int a, int b) { return a != b; }
    };

    struct EqualStringOp
    {
      typedef bool Result;
      static bool apply(const String &a, const String &b)
      {
        return a.equals(b);
      }
    };

    struct NotEqualStringOp
    {
      typedef bool Result;
      static bool apply(const String &a, const String &b)
      {
        return !a.equals(b);
      }
    };

    struct AndBoolOp
    {
      typedef bool Result;
      static bool apply(bool a, bool b) { return a && b; }
    };

    struct OrBoolOp
    {
      typedef bool Result;
      static bool apply(bool a, bool b) { return a || b; }
    };

    struct NotBoolOp
    {
      typedef bool Result;
      static bool apply(bool value) { return !value; }
    };

    struct ConcatStringOp
    {
      typedef String Result;
      static String apply(const String &a, const String &b)
      {
        return a + b;
      }
    };

    struct ConcatStringIntOp
    {
      typedef String Result;
      static String apply(const String &a, int b)
      {
        return a + String::FromInt(b);
      }
    };

    struct ConcatIntStringOp
    {
      typedef String Result;
      static String apply(int a, const String &b)
      {
        return String::FromInt(a) + b;
      }
    };

    template <typename LN, typename RN>
    inline Expr<int, BinaryExpr<AddIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator+(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<AddIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<int, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<int, BinaryExpr<SubIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator-(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<SubIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<int, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<int, BinaryExpr<MulIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator*(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<MulIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<int, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<int, BinaryExpr<DivIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator/(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<DivIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<int, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<LessIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator<(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<LessIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<GreaterIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator>(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<GreaterIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<LessEqualIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator<=(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<LessEqualIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<GreaterEqualIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator>=(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<GreaterEqualIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<EqualIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator==(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<EqualIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<NotEqualIntOp, Expr<int, LN>, Expr<int, RN> > >
    operator!=(const Expr<int, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<NotEqualIntOp, Expr<int, LN>, Expr<int, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<EqualStringOp, Expr<String, LN>, Expr<String, RN> > >
    operator==(const Expr<String, LN> &a, const Expr<String, RN> &b)
    {
      typedef BinaryExpr<EqualStringOp, Expr<String, LN>, Expr<String, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<NotEqualStringOp, Expr<String, LN>, Expr<String, RN> > >
    operator!=(const Expr<String, LN> &a, const Expr<String, RN> &b)
    {
      typedef BinaryExpr<NotEqualStringOp, Expr<String, LN>, Expr<String, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<AndBoolOp, Expr<bool, LN>, Expr<bool, RN> > >
    operator&&(const Expr<bool, LN> &a, const Expr<bool, RN> &b)
    {
      typedef BinaryExpr<AndBoolOp, Expr<bool, LN>, Expr<bool, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<bool, BinaryExpr<OrBoolOp, Expr<bool, LN>, Expr<bool, RN> > >
    operator||(const Expr<bool, LN> &a, const Expr<bool, RN> &b)
    {
      typedef BinaryExpr<OrBoolOp, Expr<bool, LN>, Expr<bool, RN> > NodeT;
      return Expr<bool, NodeT>(NodeT(a, b));
    }

    template <typename N>
    inline Expr<bool, UnaryExpr<NotBoolOp, Expr<bool, N> > >
    operator!(const Expr<bool, N> &a)
    {
      typedef UnaryExpr<NotBoolOp, Expr<bool, N> > NodeT;
      return Expr<bool, NodeT>(NodeT(a));
    }

    template <typename LN, typename RN>
    inline Expr<String, BinaryExpr<ConcatStringOp, Expr<String, LN>, Expr<String, RN> > >
    operator+(const Expr<String, LN> &a, const Expr<String, RN> &b)
    {
      typedef BinaryExpr<ConcatStringOp, Expr<String, LN>, Expr<String, RN> > NodeT;
      return Expr<String, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<String, BinaryExpr<ConcatStringIntOp, Expr<String, LN>, Expr<int, RN> > >
    operator+(const Expr<String, LN> &a, const Expr<int, RN> &b)
    {
      typedef BinaryExpr<ConcatStringIntOp, Expr<String, LN>, Expr<int, RN> > NodeT;
      return Expr<String, NodeT>(NodeT(a, b));
    }

    template <typename LN, typename RN>
    inline Expr<String, BinaryExpr<ConcatIntStringOp, Expr<int, LN>, Expr<String, RN> > >
    operator+(const Expr<int, LN> &a, const Expr<String, RN> &b)
    {
      typedef BinaryExpr<ConcatIntStringOp, Expr<int, LN>, Expr<String, RN> > NodeT;
      return Expr<String, NodeT>(NodeT(a, b));
    }

    template <typename N>
    inline Expr<int, BinaryExpr<AddIntOp, Expr<int, N>, Expr<int, ConstIntExpr> > >
    operator+(const Expr<int, N> &a, int b)
    {
      return a + Const(b);
    }

    template <typename N>
    inline Expr<int, BinaryExpr<AddIntOp, Expr<int, ConstIntExpr>, Expr<int, N> > >
    operator+(int a, const Expr<int, N> &b)
    {
      return Const(a) + b;
    }

    template <typename N>
    inline Expr<bool, BinaryExpr<GreaterEqualIntOp, Expr<int, N>, Expr<int, ConstIntExpr> > >
    operator>=(const Expr<int, N> &a, int b)
    {
      return a >= Const(b);
    }

    template <typename N>
    inline Expr<bool, BinaryExpr<LessEqualIntOp, Expr<int, N>, Expr<int, ConstIntExpr> > >
    operator<=(const Expr<int, N> &a, int b)
    {
      return a <= Const(b);
    }

    template <typename LN, typename RN>
    inline Expr<String, BinaryExpr<ConcatStringOp, Expr<String, LN>, Expr<String, ConstStrExpr> > >
    operator+(const Expr<String, LN> &a, const char *b)
    {
      return a + Const(b);
    }

    template <typename RN>
    inline Expr<String, BinaryExpr<ConcatStringOp, Expr<String, ConstStrExpr>, Expr<String, RN> > >
    operator+(const char *a, const Expr<String, RN> &b)
    {
      return Const(a) + b;
    }

  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_EXPR_HPP
