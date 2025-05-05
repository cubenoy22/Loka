#ifndef DECLARA_APP_HPP
#define DECLARA_APP_HPP

#include <vector>
#include <memory> // std::unique_ptr を使用

class Window; // 前方宣言

class App
{
public:
  App(Window *w);
  virtual ~App() = default;

  virtual void run();
  virtual void quit() = 0;
  virtual void windowClosed(Window *window);

protected:
  std::vector<Window *> windows;
  bool quitWhenLastWindowClosed_ = true;

private:
  Window *mainWindow_; // Window をポインタで保持
};

#endif // DECLARA_APP_HPP
