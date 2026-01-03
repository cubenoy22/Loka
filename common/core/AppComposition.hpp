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

  AppComposition &declare(const WindowDefinitionBase &def);

  std::vector<AppComponent *> build();

  PlatformContext *context() const { return context_; }
  void setContext(PlatformContext *ctx);

private:
  AppComposition &add(AppComponent *comp);
  std::vector<AppComponent *> components_;
  std::vector<WindowDefinitionBase *> windows_;
  PlatformContext *context_;
};

typedef AppComposition AppBuilder;

#endif // DECLARA_APP_COMPOSITION_HPP
