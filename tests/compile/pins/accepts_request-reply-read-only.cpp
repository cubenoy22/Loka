#include "app/nodes/controls/TextEditor.hpp"
void check(loka::app::scene::RequestWithReply<loka::app::LineCursor> &request)
{
  request.reply().state()->get();
}
