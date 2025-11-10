#ifndef DECLARA_CUSTOM_BUTTON_HPP
#define DECLARA_CUSTOM_BUTTON_HPP

#include "common/app2/Button.hpp"

namespace declara
{
  namespace app
  {

    struct CustomButtonProps
    {
      State<bool> *checked;
      State<std::string> *label;
      EmitterState *onClick;
      CustomButtonProps() : checked(0), label(0), onClick(0) {}
    };

    class CustomButton : public Button
    {
    public:
      CustomButtonProps props;
      CustomButton(const CustomButtonProps &p)
          : Button(ButtonProps().setText(p.label).setOnClick(p.onClick)), props(p) {}
      // ここでcheckedやlabelの状態に応じてUIやイベントを拡張できる
    };

  } // namespace app
} // namespace declara

#endif // DECLARA_CUSTOM_BUTTON_HPP
