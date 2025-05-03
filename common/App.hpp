#ifndef DECLARA_APP_HPP
#define DECLARA_APP_HPP

#include "Transaction.hpp"
#include "Property.hpp"
#include <string>
#include "Window.hpp"
#include <vector>

class Renderer;
class PageBuilder;
class Page;
class PageContext;

class UIContext
{
public:
  Renderer *renderer;
};

class App
{
public:
  App() : currentPage(0) { context.renderer = 0; }
  void setPage(Page *page)
  {
    currentPage = page;
    rerender();
  }
  void rerender()
  {
    if (currentPage)
    {
      currentPage->buildContext(&context);
      context.renderer->clearUI();
      if (Transaction::commit())
      {
        currentPage->renderAll();
      }
    }
  }
  void run()
  {
    while (true)
      context.renderer->processEvents();
  }
  void setRenderer(Renderer *r) { context.renderer = r; }

  // 🦊 新規: マルチウィンドウAPI
  virtual void addWindow(Page *page, const WindowOptions &options)
  {
    // 仮実装: Window生成はプラットフォームごとに差し替え予定
    // ここではWindow*生成・PageへのsetHostWindowを想定
    // 例: Window* w = new PlatformWindow(options);
    //     page->setHostWindow(w);
    //     w->setPage(page);
    //     windows.push_back(w);
  }

protected:
  std::vector<Window *> windows; // 追加: 全ウィンドウを管理
private:
  Page *currentPage;
  UIContext context;
};

#endif // DECLARA_APP_HPP
