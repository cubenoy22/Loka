#ifndef LOKA_TEST_TEXT_EDITOR_STATE_OWNER_HPP
#define LOKA_TEST_TEXT_EDITOR_STATE_OWNER_HPP
#include "support/Headless.hpp"
#include "app/nodes/controls/TextEditor.hpp"
namespace loka
{
  namespace app
  {
    namespace testing
    {
      /** Fixture storage uses the allocation/adoption path of state declarations. */
      class TextEditorStateOwner : private scene::HeadlessStateOwner
      {
      public:
        core::PushStateTracker &tracker;
        scene::Reported<LineCursor> cursor;
        scene::RequestWithReply<LineCursor> request;
        scene::Request<LineCursor> otherRequest;
        scene::RequestQueue<LineCursor, 4> queue;
        scene::RequestQueue<EditorCommand, 4> commands;
        TextEditorStateOwner()
            : scene::HeadlessStateOwner(),
              tracker(*scene::HeadlessStateOwner::tracker()->asPushTracker()),
              cursor(),
              request()
        {
          scene::StateBatchBase::CreateImmediateState(this, this->cursor, LineCursor::None());
          scene::StateBatchBase::CreateImmediateState(this, this->request, LineCursor::None());
          scene::StateBatchBase::CreateImmediateState(this, this->otherRequest, LineCursor::None());
          scene::StateBatchBase::CreateImmediateState(this, this->queue, LineCursor::None());
          scene::StateBatchBase::CreateImmediateState(this, this->commands, EditorCommand::None());
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka
#endif
