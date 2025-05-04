#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "Page.hpp"
#include "State.hpp"

class Page;
class Renderer;

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
  // Initialize visibility pointer in the constructor initializer list
  Window(Renderer *renderer) : renderer_(renderer), page_(0), visibility(true) {}
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

  // visibility: ウィンドウの表示/非表示状態を表す共通プロパティ
  State<bool> *visibility;

private:
  Renderer *renderer_;
  Page *page_;
};

#endif // DECLARA_WINDOW_HPP
