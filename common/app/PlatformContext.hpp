#ifndef DECLARA_PLATFORMCONTEXT_HPP
#define DECLARA_PLATFORMCONTEXT_HPP

#include <string>

class PlatformContext;

// Renderable: 描画可能なUI部品の抽象基底クラス
class Renderable
{
public:
  virtual void render(PlatformContext *context) = 0;
  virtual void updateStates() = 0;
  virtual ~Renderable() {}
};

class PlatformContext
{
public:
  virtual ~PlatformContext() {}
  virtual void processEvents() {}
  virtual void createText(const std::string &) {}
  virtual void createTextInput(void *) {}
  virtual void createButton(const std::string &, bool, void (*)()) {}
  virtual void setButtonEnabled(const std::string &, bool) {}
};

#endif // DECLARA_PLATFORMCONTEXT_HPP
