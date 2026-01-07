#ifndef LOKA_WINDOW_DEFINITION_HPP
#define LOKA_WINDOW_DEFINITION_HPP

#include <cassert>
#include "core/Window.hpp"
#include "core/PlatformContext.hpp"
#include "core2/scene/Scene.hpp"

class PlatformContext;

class WindowDefinitionBase
{
public:
  WindowDefinitionBase() : nextInComposition(0) {}
  virtual ~WindowDefinitionBase() {}
  virtual Window *create(PlatformContext *context) const = 0;
  virtual WindowDefinitionBase *clone() const = 0;

  WindowDefinitionBase *nextInComposition;
};

template <class PropsT>
struct WindowDefinition : public WindowDefinitionBase
{
#if defined(USE_LOKA_STATIC_ASSERT)
#if __cplusplus >= 201103L
  static_assert((typename PropsT::TypeTag *)0 == (typename Window::TypeTag *)0, "window_props_type_mismatch");
#else
  typedef char static_assert_window_props_type_mismatch[( (typename PropsT::TypeTag *)0 == (typename Window::TypeTag *)0 ) ? 1 : -1];
#endif
#elif !defined(NDEBUG)
#if __cplusplus >= 201103L
  static_assert((typename PropsT::TypeTag *)0 == (typename Window::TypeTag *)0, "window_props_type_mismatch");
#elif defined(LOKA_WINDOWDEF_CHECK_TYPETAG)
  typedef char static_assert_window_props_type_mismatch[( (typename PropsT::TypeTag *)0 == (typename Window::TypeTag *)0 ) ? 1 : -1];
#endif
#endif
  PropsT props;
  WindowDefinition() : props() {}
  WindowDefinition(const PropsT &p) : props(p) {}
  virtual WindowDefinitionBase *clone() const { return new WindowDefinition(*this); }
  virtual Window *create(PlatformContext *context) const
  {
    assert(context && "WindowDefinition::create requires PlatformContext");
    WindowProps resolved = props;
    if (!resolved.initialScene && resolved.rootDefinition)
    {
      declara::core::scene::NodeDefinitionBase *def = resolved.rootDefinition->clone();
      resolved.scene(new declara::core::scene::Scene(def));
    }
    return context->createWindow(resolved);
  }
};

inline WindowDefinition<WindowProps> WindowDef(const WindowProps &props)
{
  return WindowDefinition<WindowProps>(props);
}

#endif // LOKA_WINDOW_DEFINITION_HPP
