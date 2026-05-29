#ifndef LOKA_APP_COMPOSITION_HPP
#define LOKA_APP_COMPOSITION_HPP

#include <vector>
#include <cassert>
#include "app/core/AppComponent.hpp"
#include "app/core/WindowDefinition.hpp"
#include "dsl/composition/CompositionList.hpp"

class AppComposition
{
public:
  explicit AppComposition(PlatformContext *context);
  ~AppComposition();

  AppComposition &declare(const WindowDefinitionBase &def);
  AppComposition &declare(const WindowDefinitionBase *def);

  AppComposition &operator<<(const WindowDefinitionBase &def);
  AppComposition &operator<<(const WindowDefinitionBase *def);

  std::vector<AppComponent *> build();

  PlatformContext *context() const
  {
    return context_;
  }
  void setContext(PlatformContext *ctx);

private:
  AppComposition &add(AppComponent *comp);
  std::vector<AppComponent *> components_;
  loka::dsl::CompositionList<WindowDefinitionBase> windowList_;
  PlatformContext *context_;
};

typedef AppComposition AppBuilder;

#endif // LOKA_APP_COMPOSITION_HPP
