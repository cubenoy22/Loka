#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "Property.hpp"
#include "PropertyType.hpp"
#include "Page.hpp"

// WindowOptions: 動的タイトル対応・型安全な宣言的ウィンドウオプション
struct WindowOptions
{
  PropBase *title;
  WindowOptions() : title(0) {}
  template <typename T>
  WindowOptions &setTitle(T *t)
  {
    DECLARA_STATIC_STRING_PROP_GUARD(T);
    title = t;
    return *this;
  }
  // 将来: setMinimizable(bool) など拡張可
};

// Window: 完全抽象ウィンドウ（Rendererバックエンドで実装）
class Page;
class Window
{
public:
  Window(Renderer *renderer) : renderer_(renderer), page_(0) {}
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
      if (page_->commitTransaction())
      {
        page_->renderAll(renderer_);
      }
    }
  }
  Renderer *renderer() const { return renderer_; }
  Page *page() const { return page_; }

  // visibility: ウィンドウの表示/非表示状態を表す共通プロパティ
  // State<bool>やStaticProp<bool>などを想定
  BindableProp<bool> *visibility;

private:
  Renderer *renderer_;
  Page *page_;
};

#endif // DECLARA_WINDOW_HPP
