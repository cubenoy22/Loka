#ifndef DECLARA_APP_HPP
#define DECLARA_APP_HPP

#include <vector>

class Window;

class App
{
public:
  App();
  virtual ~App() = default;

  virtual void run() = 0;
  virtual void quit() = 0;
  virtual void windowClosed(Window *window);

protected:
  std::vector<Window *> windows;
  bool quitWhenLastWindowClosed_ = true;
};

#endif // DECLARA_APP_HPP
