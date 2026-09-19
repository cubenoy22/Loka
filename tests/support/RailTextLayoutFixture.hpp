#ifndef LOKA_TESTS_RAIL_TEXT_LAYOUT_FIXTURE_HPP
#define LOKA_TESTS_RAIL_TEXT_LAYOUT_FIXTURE_HPP

#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"

/** Stack fixture owns the tree; pointers borrow children from column. The fixed
    Box leg mirrors Scrapbook's page/caption/button seats; the other leg exposes
    native measured text height to Column. No view or native ownership here. */
struct RailTextLayoutFixture
{
  loka::app::StackNode column;
  loka::app::TextNode *wrapped;
  loka::app::TextNode *caption;
  loka::app::ButtonNode *button;

  RailTextLayoutFixture(bool boxed, const char *text)
      : column(loka::app::StackProps(loka::app::STACK_AXIS_COLUMN))
  {
    using namespace loka::app;
    TextProps page(text);
    page.textStyle_ = FontSize<18>();
    page.blockStyle_ = BlockStyle().wrap(TEXT_WRAP_WORD).truncation(TEXT_TRUNCATION_NONE);
    this->wrapped = new TextNode(page);
    this->caption = new TextNode(TextProps("Page 5"));
    this->button = new ButtonNode(ButtonProps());
    if (boxed)
    {
      BoxNode *box = new BoxNode(BoxProps().setSize(300, 170));
      box->addChild(this->wrapped);
      this->column.addChild(box);
      StackNode *captionRow = new StackNode(StackProps(STACK_AXIS_ROW));
      captionRow->props.alignVertical(VERTICAL_ALIGNMENT_CENTER);
      captionRow->addChild(this->caption);
      TextProps badge("TXT");
      badge.textStyle_ = Bold;
      captionRow->addChild(new TextNode(badge));
      this->column.addChild(captionRow);
      StackNode *buttonRow = new StackNode(StackProps(STACK_AXIS_ROW));
      buttonRow->addChild(this->button);
      buttonRow->addChild(new ButtonNode(ButtonProps()));
      this->column.addChild(buttonRow);
    }
    else
    {
      this->column.addChild(this->wrapped);
      this->column.addChild(this->caption);
      this->column.addChild(this->button);
    }
  }

private:
  RailTextLayoutFixture(const RailTextLayoutFixture &);
  RailTextLayoutFixture &operator=(const RailTextLayoutFixture &);
};

#endif
