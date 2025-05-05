#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "Page.hpp"
#include "State.hpp"

class Page;
class Renderer;
class App;

// WindowOptions: 動的タイトル対応・型安全な宣言的ウィンドウオプション
struct WindowOptions
{
  State<std::string> *title;
  WindowOptions() : title(0) {}
  WindowOptions &setTitle(State<std::string> *t)
  {
    title = t;
    return *this;
  }
  // 将来: setMinimizable(bool) など拡張可
};

class Window
{
public:
  // Windowクラスのコンストラクタでvisibilityを適切に初期化
  Window(Renderer *renderer, App *app) : renderer_(renderer), page_(0), visibility(true), app_(app) {}
  virtual ~Window() = default;

  void setPage(Page *page)
  {
    page_ = page;
    rerender();
  }
  void rerender()
  {
    if (page_)
    {
      page_->buildContext();
      page_->renderAll(renderer_);
    }
  }
  Renderer *renderer() const { return renderer_; }
  Page *page() const { return page_; }

  void setApp(App *app) { app_ = app; }
  App *getApp() const { return app_; }

  // visibility: ウィンドウの表示/非表示状態を表す共通プロパティ
  MutableState<bool> visibility;

private:
  Renderer *renderer_;
  Page *page_;
  App *app_; // App をポインタで保持
};

#endif // DECLARA_WINDOW_HPP
