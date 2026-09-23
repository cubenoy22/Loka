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
        scene::NodeState<LineCursor> request;
        TextEditorStateOwner()
            : scene::HeadlessStateOwner(),
              tracker(*scene::HeadlessStateOwner::tracker()->asPushTracker()),
              cursor(),
              request()
        {
          scene::StateBatchBase::CreateImmediateState(this, this->cursor, LineCursor::None());
          scene::StateBatchBase::CreateImmediateState(this, this->request, LineCursor::None());
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka
#endif
