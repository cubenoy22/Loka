#include "AttrDslTests.hpp"

#include <cassert>
#include <cstdio>

#include "app/ImageView.hpp"
#include "app/Menu.hpp"
#include "app/Text.hpp"
#include "loka/core/State.hpp"

void testLokaAttrDslV1Core()
{
  printf("\n==== [testLokaAttrDslV1Core] start ====\n");

  // --- v1 attr storage: Text/ImageView should preserve attr on props ---
  {
    loka::core::MutableState<int> dynamicFontSize(22);
    loka::app::TextDefinitionWithAttr text = loka::app::Text("Hello").attr(
        loka::app::TextAttr().fontSize(&dynamicFontSize).weight(loka::app::TEXT_WEIGHT_BOLD));
    assert(text.props.hasAttr_);
    assert(text.props.attr_.fontSizeState_ == &dynamicFontSize);
    assert(text.props.attr_.hasWeightValue_);
    assert(text.props.attr_.weightValue_ == loka::app::TEXT_WEIGHT_BOLD);

    loka::app::ImageViewDefinitionWithAttr image = loka::app::ImageView().attr(
        loka::app::ImageViewAttr().fit(loka::app::IMAGE_FIT_CONTAIN));
    assert(image.props.hasAttr_);
    assert(image.props.attr_.hasFitValue_);
    assert(image.props.attr_.fitValue_ == loka::app::IMAGE_FIT_CONTAIN);
  }

  // --- v1 attr copy safety (POD): copy should stay independent ---
  {
    loka::app::TextAttr a;
    loka::app::TextAttr b = a;
    a.fontSize(14);
    assert(!b.hasFontSizeValue_);
    assert(a.hasFontSizeValue_);
    assert(a.fontSizeValue_ == 14);
  }

  // --- MenuItem attr participates in structure equality ---
  {
    loka::core::MutableState<bool> disabledState(true);
    loka::app::MenuItemDefinition left("Open");
    left.attr(loka::app::MenuItemAttr().disabled(&disabledState));

    loka::app::MenuItemDefinition right("Open");
    right.attr(loka::app::MenuItemAttr().disabled(&disabledState));
    assert(left.equalsStructure(right));

    loka::app::MenuItemDefinition changed("Open");
    changed.attr(loka::app::MenuItemAttr().disabled(false));
    assert(!left.equalsStructure(changed));
  }

  // --- MenuItem disabled attr -> effective enabled projection ---
  {
    loka::app::MenuItemDefinition disabledByValue("Open");
    disabledByValue.attr(loka::app::MenuItemAttr().disabled(true));
    assert(!disabledByValue.isEnabledInitial());
    assert(disabledByValue.enabledBindingState() == 0);

    loka::core::MutableState<bool> disabledState(false);
    loka::app::MenuItemDefinition disabledByState("Open");
    disabledByState.attr(loka::app::MenuItemAttr().disabled(&disabledState));
    assert(disabledByState.isEnabledInitial());
    assert(disabledByState.enabledBindingState() == &disabledState);
    assert(disabledByState.enabledBindingInvert());
    disabledState.set(true);
    assert(!disabledByState.isEnabledInitial());

    loka::core::MutableState<bool> enabledState(true);
    loka::app::MenuItemDefinition explicitEnabled("Open");
    explicitEnabled.enabled(&enabledState).attr(loka::app::MenuItemAttr().disabled(true));
    assert(explicitEnabled.enabledBindingState() == &enabledState);
    assert(!explicitEnabled.enabledBindingInvert());
    assert(explicitEnabled.isEnabledInitial());
    enabledState.set(false);
    assert(!explicitEnabled.isEnabledInitial());
  }

  printf("==== [testLokaAttrDslV1Core] end ====\n");
}
