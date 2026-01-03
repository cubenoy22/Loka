#ifndef DECLARA_APP_COMPOSITION_HPP
#define DECLARA_APP_COMPOSITION_HPP

#include <vector>
#include <cassert>
#include "core/AppComponent.hpp"
#include "core/WindowDefinition.hpp"

class AppComposition
{
public:
  explicit AppComposition(PlatformContext *context);
  ~AppComposition();

  AppComposition &add(AppComponent *comp);
  AppComposition &Window(const WindowProps &props);
  AppComposition &declare(const WindowDefinitionBase &def);

  std::vector<AppComponent *> build();

  PlatformContext *context() const { return context_; }
  void setContext(PlatformContext *ctx);

private:
  std::vector<AppComponent *> components_;
  std::vector<WindowDefinitionBase *> windows_;
  PlatformContext *context_;
};

typedef AppComposition AppBuilder;

#endif // DECLARA_APP_COMPOSITION_HPP
