#ifndef DECLARA_BUTTON_HPP
#define DECLARA_BUTTON_HPP

#include <string>
#include <vector>
#include "Property.hpp"
#include "Renderer.hpp"
#include "PropertyUtil.hpp"
#include "PropertyType.hpp"

struct ButtonOptions
{
  std::string label;
  PropBase *enabled;
  void (*onClick)();

public:
  ButtonOptions() : enabled(&TRUE), onClick(0) {}
  ButtonOptions &setLabel(const std::string &l)
  {
    label = l;
    return *this;
  }
  template <typename T>
  ButtonOptions &setEnabled(T *e)
  {
    DECLARA_STATIC_BOOL_PROP_GUARD(T);
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
  ButtonComponent(const std::string &label, PropBase *enabled, void (*onClick)())
      : label(label), enabled(enabled), onClick(onClick) {}
  void render(Renderer *renderer)
  {
    bool isEnabled = getBoolProp(enabled);
    renderer->createButton(label, isEnabled, onClick);
  }
  void updateProps() {}
  void setEnabledProp(PropBase *prop) { enabled = prop; }
  std::string label;
  PropBase *enabled;
  void (*onClick)();
};

#endif // DECLARA_BUTTON_HPP
