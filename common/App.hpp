#ifndef DECLARA_APP_HPP
#define DECLARA_APP_HPP

#include "Transaction.hpp"
#include "Property.hpp"
#include <string>
#include "Window.hpp"
#include <vector>
#include "PlatformContext.hpp"
#include "EmptyContext.hpp"

class Renderer;
class PageBuilder;
class Page;
class PageContext;

class App
{
public:
  App(Window* w) : window(w) {}
  // 必要ならウィンドウ管理APIのみ残す
  virtual void addWindow(Page *page, const WindowOptions &options)
  {
    // 仮実装: Window生成はプラットフォームごとに差し替え予定
    // ここではWindow*生成・PageへのsetHostWindowを想定
    // 例: Window* w = new PlatformWindow(options);
    //     page->setHostWindow(w);
    //     w->setPage(page);
    //     windows.push_back(w);
  }
  void run() {
    // 必要に応じて初回描画
    window->rerender();
    while (true) window->renderer()->processEvents();
  }

protected:
  std::vector<Window *> windows; // 複数ウィンドウ対応用
private:
  Window* window;
};

#endif // DECLARA_APP_HPP
