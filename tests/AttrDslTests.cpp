#include "AttrDslTests.hpp"

#include <cassert>
#include <cstdio>

#include "app/ImageView.hpp"
#include "app/Menu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/layout/LayoutHeuristics.hpp"
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
    assert(!text.props.attr_.hasWrapValue_);
    assert(!text.props.attr_.hasTruncationValue_);

    loka::app::ImageViewDefinitionWithAttr image = loka::app::ImageView().attr(
        loka::app::ImageViewAttr()
            .sizePolicy(loka::app::IMAGE_VIEW_SIZE_FILL_PARENT)
            .fit(loka::app::IMAGE_FIT_CONTAIN));
    assert(image.props.hasAttr_);
    assert(image.props.attr_.hasFitValue_);
    assert(image.props.attr_.fitValue_ == loka::app::IMAGE_FIT_CONTAIN);
    assert(image.props.attr_.hasSizePolicyValue_);
    assert(image.props.attr_.sizePolicyValue_ == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT);
  }

  // --- Text overflow attr storage ---
  {
    loka::app::TextDefinitionWithAttr text = loka::app::Text("Hello").attr(
        loka::app::TextAttr()
            .wrap(loka::app::TEXT_WRAP_WORD)
            .truncation(loka::app::TEXT_TRUNCATION_ELLIPSIS));
    assert(text.props.hasAttr_);
    assert(text.props.attr_.hasWrapValue_);
    assert(text.props.attr_.wrapValue_ == loka::app::TEXT_WRAP_WORD);
    assert(text.props.attr_.hasTruncationValue_);
    assert(text.props.attr_.truncationValue_ == loka::app::TEXT_TRUNCATION_ELLIPSIS);

    loka::app::TextDefinitionWithAttr charWrap = loka::app::Text("Path").attr(
        loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_CHAR));
    assert(charWrap.props.attr_.hasWrapValue_);
    assert(charWrap.props.attr_.wrapValue_ == loka::app::TEXT_WRAP_CHAR);
  }

  // --- Row/Column alignment props storage ---
  {
    loka::app::VStack column = loka::app::VStack().alignHorizontal(loka::app::HORIZONTAL_ALIGNMENT_CENTER);
    assert(column.props.hasHorizontalAlignment_);
    assert(column.props.horizontalAlignment_ == loka::app::HORIZONTAL_ALIGNMENT_CENTER);

    loka::app::HStack row = loka::app::HStack().alignVertical(loka::app::VERTICAL_ALIGNMENT_BOTTOM);
    assert(row.props.hasVerticalAlignment_);
    assert(row.props.verticalAlignment_ == loka::app::VERTICAL_ALIGNMENT_BOTTOM);
  }

  // --- Column remaining-height helper ---
  {
    assert(loka::app::layout::remainingChildHeightForColumn(400, 20, 20) == 400);
    assert(loka::app::layout::remainingChildHeightForColumn(400, 20, 120) == 300);
    assert(loka::app::layout::remainingChildHeightForColumn(400, 20, 999) == 0);
    assert(loka::app::layout::remainingChildHeightForColumn(0, 20, 50) == 0);
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
    loka::app::MenuItemDefinitionWithAttr left =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(&disabledState));

    loka::app::MenuItemDefinitionWithAttr right =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(&disabledState));
    assert(left.equalsStructure(right));

    loka::app::MenuItemDefinitionWithAttr changed =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(false));
    assert(!left.equalsStructure(changed));
  }

  // --- MenuItem.attr is chain-style: returned definition carries attr ---
  {
    loka::app::MenuItemDefinitionWithAttr item =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(true));
    assert(item.hasAttr_);
    assert(item.attr_.hasDisabledValue_);
    assert(item.attr_.disabledValue_);
  }

  // --- MenuItem disabled attr -> effective enabled projection ---
  {
    loka::app::MenuItemDefinitionWithAttr disabledByValue =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(true));
    assert(!disabledByValue.isEnabledInitial());
    assert(disabledByValue.enabledBindingState() == 0);

    loka::core::MutableState<bool> disabledState(false);
    loka::app::MenuItemDefinitionWithAttr disabledByState =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().disabled(&disabledState));
    assert(disabledByState.isEnabledInitial());
    assert(disabledByState.enabledBindingState() == &disabledState);
    assert(disabledByState.enabledBindingInvert());
    disabledState.set(true);
    assert(!disabledByState.isEnabledInitial());

    loka::core::MutableState<bool> enabledState(true);
    loka::app::MenuItemDefinitionWithAttr explicitEnabled =
        loka::app::MenuItem("Open").enabled(&enabledState).attr(loka::app::MenuItemAttr().disabled(true));
    assert(explicitEnabled.enabledBindingState() == &enabledState);
    assert(!explicitEnabled.enabledBindingInvert());
    assert(explicitEnabled.isEnabledInitial());
    enabledState.set(false);
    assert(!explicitEnabled.isEnabledInitial());
  }

  // --- MenuItem visible projection ---
  {
    loka::app::MenuItemDefinitionWithAttr hiddenByValue =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().visible(false));
    assert(!hiddenByValue.isVisibleInitial());

    loka::core::MutableState<bool> visibleState(false);
    loka::app::MenuItemDefinitionWithAttr hiddenByState =
        loka::app::MenuItem("Open").attr(loka::app::MenuItemAttr().visible(&visibleState));
    assert(!hiddenByState.isVisibleInitial());
    visibleState.set(true);
    assert(hiddenByState.isVisibleInitial());
  }

  printf("==== [testLokaAttrDslV1Core] end ====\n");
}
