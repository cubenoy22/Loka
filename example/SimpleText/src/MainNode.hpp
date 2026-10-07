#ifndef LOKA_SIMPLE_TEXT_MAIN_NODE_HPP
#define LOKA_SIMPLE_TEXT_MAIN_NODE_HPP

#include <new>
#include "app/scene/BorrowedKeys.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/nestable/PolicyScope.hpp"
#include "app/nodes/Text.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/TextDocumentFile.hpp"
#include "app/scene/state/FlowSlot.hpp"

#ifdef TEST_BUILD
class SimpleTextTestAccess;
#endif

namespace simpletext
{
  enum Operation
  {
    NONE,
    OPEN,
    SAVE
  };
  class MainTypeTag
  {
  };
  class MainNode;

  struct MainProps : loka::app::scene::NodePropsBase<MainProps>
  {
    typedef MainTypeTag TypeTag;
    typedef MainNode NodeType;
    MainProps &platformContext(PlatformContext *value)
    {
      this->keys_.set(PLATFORM, value);
      return *this;
    }
    PlatformContext *platformContext() const
    {
      return static_cast<PlatformContext *>(const_cast<void *>(this->keys_.get(PLATFORM)));
    }
    MainProps &newEvent(loka::core::EmitterState *value)
    {
      this->keys_.set(NEW_EVENT, value);
      return *this;
    }
    MainProps &openEvent(loka::core::EmitterState *value)
    {
      this->keys_.set(OPEN_EVENT, value);
      return *this;
    }
    MainProps &saveEvent(loka::core::EmitterState *value)
    {
      this->keys_.set(SAVE_EVENT, value);
      return *this;
    }
    MainProps &saveAsEvent(loka::core::EmitterState *value)
    {
      this->keys_.set(SAVE_AS_EVENT, value);
      return *this;
    }
    loka::core::EmitterState *newEvent() const
    {
      return this->event(NEW_EVENT);
    }
    loka::core::EmitterState *openEvent() const
    {
      return this->event(OPEN_EVENT);
    }
    loka::core::EmitterState *saveEvent() const
    {
      return this->event(SAVE_EVENT);
    }
    loka::core::EmitterState *saveAsEvent() const
    {
      return this->event(SAVE_AS_EVENT);
    }
    void assertInitialized() const
    {
      assert(this->keys_.complete());
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() && this->keys_ < static_cast<const MainProps &>(rhs).keys_;
    }

  private:
    enum Key
    {
      PLATFORM,
      NEW_EVENT,
      OPEN_EVENT,
      SAVE_EVENT,
      SAVE_AS_EVENT,
      KEY_COUNT
    };
    loka::core::EmitterState *event(Key key) const
    {
      return static_cast<loka::core::EmitterState *>(const_cast<void *>(this->keys_.get(key)));
    }
    loka::app::scene::BorrowedKeys<KEY_COUNT> keys_;
  };

  /** One document owner; chooser facts arrive through separate rail-written channels. */
  class MainNode : public loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>
  {
    typedef loka::app::FileChooserResult Choice;
    typedef loka::dsl::FlowChain<Choice, Choice> ChoiceFlow;
    enum DocumentAction
    {
      NEW_DOCUMENT,
      OPEN_DOCUMENT,
      SAVE_DOCUMENT
    };

    class IsOperation : public loka::core::DerivedState<bool>::EvalFn
    {
    public:
      IsOperation(const loka::app::scene::NodeState<Operation> &source, Operation value)
          : source_(source),
            value_(value)
      {
      }
      virtual bool operator()()
      {
        return this->source_.get() == this->value_;
      }

    private:
      const loka::app::scene::NodeState<Operation> &source_;
      const Operation value_;
    };

    /** Replay the owner's old IDs before inserting the sole empty row. */
    class EmptyDocument : public loka::core::ListOpCursor<loka::core::String>
    {
    public:
      explicit EmptyDocument(const loka::core::ObservableList<loka::core::String> &lines)
          : lines_(lines),
            position_(0)
      {
      }
      virtual void rewind()
      {
        this->position_ = 0;
      }
      virtual bool next(loka::core::ListOp<loka::core::String> &out)
      {
        using namespace loka::core;
        if (this->position_ < this->lines_.size())
          out = ListOp<String>(REMOVE, this->lines_.at(this->lines_.size() - 1 - this->position_).id);
        else if (this->position_ == this->lines_.size())
          out = ListOp<String>(INSERT, ItemId::none(), 0, String());
        else
          return false;
        ++this->position_;
        return true;
      }

    private:
      const loka::core::ObservableList<loka::core::String> &lines_;
      unsigned short position_;
    };

    struct ChooserCompletion
    {
      typedef Choice In;
      typedef Choice Out;
      loka::dsl::StepRunStatus run(const In &in, Out &out, loka::dsl::FlowError &) const
      {
        out = in;
        return loka::dsl::FLOW_STEP_SUCCEEDED;
      }
    };

  public:
    typedef MainTypeTag TypeTag;
    explicit MainNode(const MainProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<MainProps>(props),
          openFlow_(*this),
          saveFlow_(*this)
    {
      this->state(this->operation_, NONE);
      this->state(this->openResult_, Choice());
      this->state(this->saveResult_, Choice());
      this->state(this->error_, loka::core::String());
      this->state(this->cursor_, loka::app::LineCursor::None());
      this->state(this->caret_, loka::app::LineCursor::None());
      this->derived(this->opening_, this->operation_, new (std::nothrow) IsOperation(this->operation_, OPEN));
      this->derived(this->saving_, this->operation_, new (std::nothrow) IsOperation(this->operation_, SAVE));
    }

    virtual void attachNode(loka::app::scene::NodeComposition &)
    {
      loka::core::StateTracker *owner = 0;
      if (this->lines_.queryMutationTracker(owner) != loka::core::EDIT_OK)
      {
        if (this->lines_.attach(this->tracker()->asPushTracker(), loka::app::TextEditorProps::kMaxLines)
            != loka::core::ATTACH_OK)
          this->error_.set(loka::core::String::Literal("Cannot allocate document."));
        else
          this->commitDocument(NEW_DOCUMENT, Choice());
      }
      this->openFlow_
          .set(loka::dsl::Flow() | loka::dsl::Step(1, ChooserCompletion()).onSuccess(&MainNode::opened, this))
          .bindTrigger(this->openResult_.state())
          .withTracker(this->tracker());
      this->saveFlow_.set(loka::dsl::Flow() | loka::dsl::Step(1, ChooserCompletion()).onSuccess(&MainNode::saved, this))
          .bindTrigger(this->saveResult_.state())
          .withTracker(this->tracker());
    }

    virtual void declareBindings(loka::app::scene::BindingToken &t)
    {
      this->props.assertInitialized();
      t.action(*this->props.newEvent(), this, &MainNode::newDocument);
      t.action(*this->props.openEvent(), this, &MainNode::openDocument);
      t.action(*this->props.saveEvent(), this, &MainNode::saveDocument);
      t.action(*this->props.saveAsEvent(), this, &MainNode::saveAsDocument);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(HStack()
                << (VStack()
                    << Text(this->error_.state()).TEST_ID("SimpleText.Error")
                    << TextEditor(this->lines_, this->cursor_).moveCaretTo(this->caret_).TEST_ID("SimpleText.Editor"))
                << (Show(*this->opening_.state()) << (PolicyScopeDefinition().destroyOnDetach()
                                                      << OpenFileDialog()
                                                             .filterPolicy(FILE_DIALOG_FILTER_ALL_FILES_TEXT)
                                                             .result(this->openResult_)
                                                             .testId("SimpleText.Open")))
                << (Show(*this->saving_.state()) << (PolicyScopeDefinition().destroyOnDetach()
                                                     << SaveFileDialog(loka::core::String::Literal("untitled.txt"))
                                                            .filterPolicy(FILE_DIALOG_FILTER_ALL_FILES_TEXT)
                                                            .result(this->saveResult_)
                                                            .testId("SimpleText.Save"))));
    }

  private:
#ifdef TEST_BUILD
    friend class ::SimpleTextTestAccess;
#endif
    void newDocument()
    {
      if (this->operation_.get() == NONE)
        this->commitDocument(NEW_DOCUMENT, Choice());
    }
    void openDocument()
    {
      if (this->operation_.get() == NONE)
        this->operation_.set(OPEN);
    }
    void saveAsDocument()
    {
      if (this->operation_.get() == NONE)
        this->operation_.set(SAVE);
    }
    void saveDocument()
    {
      if (this->operation_.get() != NONE)
        return;
      if (this->currentFile_.kind == Choice::RESULT_FILE)
        this->commitDocument(SAVE_DOCUMENT, this->currentFile_);
      else
        this->saveAsDocument();
    }

    static void opened(const Choice &result, void *owner)
    {
      static_cast<MainNode *>(owner)->completeChooser(OPEN, result);
    }
    static void saved(const Choice &result, void *owner)
    {
      static_cast<MainNode *>(owner)->completeChooser(SAVE, result);
    }
    void completeChooser(Operation expected, const Choice &result)
    {
      if (this->operation_.get() != expected || result.kind == Choice::RESULT_NONE)
        return;
      this->operation_.set(NONE);
      switch (result.kind)
      {
      case Choice::RESULT_FILE:
        this->commitDocument(expected == OPEN ? OPEN_DOCUMENT : SAVE_DOCUMENT, result);
        break;
      case Choice::RESULT_FOLDER:
      case Choice::RESULT_ERROR:
        this->error_.set(loka::core::String::Literal("File dialog failed."));
        break;
      case Choice::RESULT_CANCELED:
      case Choice::RESULT_NONE:
        break;
      }
    }

    static loka::core::String failureText(DocumentAction action, loka::app::TextDocumentResult result)
    {
      using namespace loka::app;
      switch (action)
      {
      case NEW_DOCUMENT:
        return loka::core::String::Literal("Cannot start a new document.");
      case OPEN_DOCUMENT:
        if (result == TEXT_DOCUMENT_TOO_LARGE)
          return loka::core::String::Literal("Cannot open the file: it is too long for the editor.");
        if (result == TEXT_DOCUMENT_NON_ASCII)
          return loka::core::String::Literal("Cannot open the file: it is not plain ASCII text.");
        return loka::core::String::Literal("Cannot open the file.");
      case SAVE_DOCUMENT:
        // The prepare door refuses a non-TEXT file before anything is written.
        if (result == TEXT_DOCUMENT_NOT_TEXT)
          return loka::core::String::Literal("Not saved: the file is not a text file.");
        return loka::core::String::Literal("Save failed; the destination may have changed.");
      }
      return loka::core::String();
    }

    /** The sole destination writer. Replacement success also supersedes stale caret IDs. */
    void commitDocument(DocumentAction action, const Choice &destination)
    {
      using namespace loka::app;
      loka::core::StateTrackerGuard guard(this->tracker());
      bool committed = false;
      TextDocumentResult result = TEXT_DOCUMENT_OK;
      switch (action)
      {
      case NEW_DOCUMENT:
      {
        EmptyDocument replacement(this->lines_);
        committed = this->lines_.apply(replacement) == loka::core::EDIT_OK;
        break;
      }
      case OPEN_DOCUMENT:
        result = ReadTextDocument(this->props.platformContext(), destination.item, this->lines_);
        committed = result == TEXT_DOCUMENT_OK;
        break;
      case SAVE_DOCUMENT:
        result = WriteTextDocument(this->props.platformContext(), destination.item, this->lines_);
        committed = result == TEXT_DOCUMENT_OK;
        break;
      }
      if (!committed)
      {
        this->error_.set(failureText(action, result));
        return;
      }
      this->currentFile_ = destination;
      this->error_.set(loka::core::String());
      if (action != SAVE_DOCUMENT)
        this->caret_.set(LineCursor(this->lines_.at(0).id, 0));
    }

    loka::core::ObservableList<loka::core::String> lines_;
    loka::app::scene::Reported<loka::app::LineCursor> cursor_;
    loka::app::scene::Request<loka::app::LineCursor> caret_;
    Choice currentFile_;
    loka::app::scene::NodeState<Operation> operation_;
    loka::app::scene::DerivedNodeState<bool> opening_, saving_;
    loka::app::scene::NodeState<Choice> openResult_, saveResult_;
    loka::app::scene::NodeState<loka::core::String> error_;
    loka::app::scene::FlowSlot<ChoiceFlow> openFlow_, saveFlow_;
  };
} // namespace simpletext
#endif
