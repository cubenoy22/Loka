#ifndef DECLARA_APP_HPP
#define DECLARA_APP_HPP

#include "Transaction.hpp"
#include "Property.hpp"
#include <string>

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

private:
  Page *currentPage;
  UIContext context;
};

#endif // DECLARA_APP_HPP
