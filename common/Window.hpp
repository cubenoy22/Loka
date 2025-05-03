#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "Property.hpp"
#include "PropertyType.hpp"

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
  virtual ~Window() {}
  virtual void setPage(Page *page) = 0;
  // 将来: WindowIDやネイティブハンドル取得APIも追加可
};

#endif // DECLARA_WINDOW_HPP
