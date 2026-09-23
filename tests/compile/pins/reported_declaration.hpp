#ifndef LOKA_COMPILE_REPORTED_DECLARATION_HPP
#define LOKA_COMPILE_REPORTED_DECLARATION_HPP
#include "app/nodes/controls/TextEditor.hpp"
#include "app/scene/node/ComposableNode.hpp"
/** Instantiate both ordinary declaration doors; no mutable alias is returned. */
class ReportedDeclarationProbe : public loka::app::scene::ComposableNode
{
public:
  ReportedDeclarationProbe()
  {
    this->state(this->single_, loka::app::LineCursor::None());
    this->declareStates(1).state(this->batched_, loka::app::LineCursor::None());
  }

private:
  loka::app::scene::Reported<loka::app::LineCursor> single_;
  loka::app::scene::Reported<loka::app::LineCursor> batched_;
};
#endif
