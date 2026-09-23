#include "reported_declaration.hpp"
void probe(loka::core::ObservableList<loka::core::String> &lines,
           loka::app::scene::Reported<loka::app::LineCursor> &reported)
{
  (void)lines;
  (void)reported;
  (void)loka::app::TextEditorProps(lines, reported);
}
