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
      class TextEditorStateOwner
      {
      private:
        scene::HeadlessStateOwner owner_;

      public:
        core::PushStateTracker &tracker;
        scene::Reported<LineCursor> cursor;
        scene::NodeState<LineCursor> request;
        TextEditorStateOwner()
            : owner_(),
              tracker(*owner_.tracker()->asPushTracker()),
              cursor(),
              request()
        {
          scene::StateBatchBase::CreateImmediateState(&this->owner_, this->cursor, LineCursor::None());
          scene::StateBatchBase::CreateImmediateState(&this->owner_, this->request, LineCursor::None());
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka
#endif
