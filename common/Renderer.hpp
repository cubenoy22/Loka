#ifndef DECLARA_RENDERER_HPP
#define DECLARA_RENDERER_HPP

#include <string>

// Renderable: 描画可能なUI部品の抽象基底クラス
class Renderable
{
public:
  virtual void render(Renderer *renderer) = 0;
  virtual void updateProps() = 0;
  virtual ~Renderable() {}
};

class Renderer
{
public:
  virtual ~Renderer() {}
  virtual void clearUI() {}
  virtual void processEvents() {}
  virtual void createText(const std::string &) {}
  virtual void createTextInput(void *) {}
  virtual void createButton(const std::string &, bool, void (*)()) {}
  virtual void setButtonEnabled(const std::string &, bool) {}
  virtual void showMessage(const std::string &) {}
};

#endif // DECLARA_RENDERER_HPP
