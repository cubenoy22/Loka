#ifndef DECLARA_APPCOMPONENT_HPP
#define DECLARA_APPCOMPONENT_HPP

#include <vector>
#include <stdexcept>
#include <cassert>

// 前方宣言（循環include防止）
class Window;
class PlatformContext;
struct WindowOptions;

// --- AppComponent: アプリ全体の構成要素の基底クラス（今後拡張可） ---
class AppComponent
{
public:
  virtual ~AppComponent() {}
};

// --- AppBuilder: AppComponentをDSL的に追加できるビルダー ---
class AppBuilder
{
public:
  explicit AppBuilder(PlatformContext *context)
      : context_(context)
  {
    assert(context_ && "AppBuilder: PlatformContext* must not be nullptr");
  }

  AppBuilder &add(AppComponent *comp)
  {
    components_.push_back(comp);
    return *this;
  }

  AppBuilder &Window(const WindowOptions &opts); // 実装を別ファイルに分離

  std::vector<AppComponent *> build()
  {
    return components_;
  }

  PlatformContext *context() const
  {
    return context_;
  }

  void setContext(PlatformContext *ctx)
  {
    context_ = ctx;
  }

private:
  std::vector<AppComponent *> components_;
  PlatformContext *context_;
};

#endif // DECLARA_APPCOMPONENT_HPP
