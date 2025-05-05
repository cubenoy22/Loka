#ifndef DECLARA_BUTTON_HPP
#define DECLARA_BUTTON_HPP

#include <string>
#include "core/State.hpp"
#include "app/Renderer.hpp"

// グローバル定数State（常に有効なボタン用）
static State<bool> BUTTON_DEFAULT_ENABLED(true);

struct ButtonOptions
{
  std::string label;
  State<bool> *enabled;
  void (*onClick)();

  ButtonOptions() : label(""), enabled(&BUTTON_DEFAULT_ENABLED), onClick(NULL) {}
  ButtonOptions &setLabel(const std::string &l)
  {
    label = l;
    return *this;
  }
  template <typename T>
  ButtonOptions &setEnabled(T *e)
  {
    enabled = e;
    return *this;
  }
  ButtonOptions &setOnClick(void (*cb)())
  {
    onClick = cb;
    return *this;
  }
};

// Component: すべてのUI部品の共通基底クラス
class Component : public Renderable
{
public:
  virtual ~Component() {}
};

class ButtonComponent : public Component
{
public:
  ButtonComponent(const std::string &label, State<bool> *enabled, void (*onClick)())
      : label(label), enabled(enabled), onClick(onClick) {}
  void render(Renderer *renderer)
  {
    bool isEnabled = enabled ? enabled->get() : true;
    renderer->createButton(label, isEnabled, onClick);
  }
  void updateStates() {}
  void setEnabledState(State<bool> *state) { enabled = state; }
  std::string label;
  State<bool> *enabled;
  void (*onClick)();
};

#endif // DECLARA_BUTTON_HPP
