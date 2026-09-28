#ifndef LOKA_NULL_INPUT_DOOR_HPP
#define LOKA_NULL_INPUT_DOOR_HPP

#include "platform/null/context/NullScrollBarContext.hpp"
#include "platform/null/context/NullTextEditorContext.hpp"

/** Synchronous Null input operations. The invocation restores the previous
    controller phase; only the existing owner pump advances held Scene work.
    Callers supply a live receiver and do not borrow it after a retiring input. */
class NullInputDoor
{
private:
  /** One scope-owning implementation for all typed arities. The temporary
      encloses the whole call expression, including the body's continuation. */
  class Invocation : private loka::app::scene::OperationScope
  {
  public:
    explicit Invocation(loka::app::scene::IPlatformController &controller)
        : loka::app::scene::OperationScope(controller) {}

    template<class C, class R>
    R operator()(C &c, R (C::*body)()) const
    { return (c.*body)(); }
    template<class C, class R, class A>
    R operator()(C &c, R (C::*body)(A), A a) const
    { return (c.*body)(a); }
    template<class C, class R, class A, class B>
    R operator()(C &c, R (C::*body)(A, B), A a, B b) const
    { return (c.*body)(a, b); }
    template<class C, class R, class A, class B, class D>
    R operator()(C &c, R (C::*body)(A, B, D), A a, B b, D d) const
    { return (c.*body)(a, b, d); }
  };

public:
  static void simulatePress(NullScrollBarContext &c, NullScrollBarContext::Part part, int repeats)
  { Invocation(*c.controller_)(c, &NullScrollBarContext::simulatePress, part, repeats); }
  static void simulateThumbDragTo(NullScrollBarContext &c, int value)
  { Invocation(*c.controller_)(c, &NullScrollBarContext::simulateThumbDragTo, value); }
  static void pressTick(NullScrollBarContext &c, NullScrollBarContext::Part part)
  { Invocation(*c.controller_)(c, &NullScrollBarContext::pressTick, part); }
  static void dragThumbTo(NullScrollBarContext &c, int value)
  { Invocation(*c.controller_)(c, &NullScrollBarContext::dragThumbTo, value); }
  static void release(NullScrollBarContext &c)
  { Invocation(*c.controller_)(c, &NullScrollBarContext::release); }
  static loka::app::EditorResult textEditorInput(NullTextEditorContext &c,
      const std::string &bytes, bool join, const loka::app::LineCursor *move)
  {
    return Invocation(c.controller_).operator()<NullTextEditorContext,
        loka::app::EditorResult, const std::string &, bool, const loka::app::LineCursor *>(
            c, &NullTextEditorContext::input, bytes, join, move);
  }
};
#endif
