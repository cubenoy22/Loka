#ifndef SMIRKYCARD_CARD_RECORDS_HPP
#define SMIRKYCARD_CARD_RECORDS_HPP

#include "ScriptRuntime.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/LokaAlloc.hpp"

namespace smirkycard
{
  enum
  {
    kCardSeatBudget = 128,
    kCardClickableBudget = 128
  };

  /** Card-owned intrusive storage. Records never move; no cached length. */
  template <class T> class CardRecords
  {
  public:
    explicit CardRecords(const char *type)
        : head_(0),
          site_("JsCardNode", type)
    {
    }
    ~CardRecords()
    {
      while (this->head_)
      {
        T *next = this->head_->next;
        loka::core::LokaDelete(this->head_, this->site_);
        this->head_ = next;
      }
    }
    T *head() const
    {
      return this->head_;
    }
    unsigned count() const
    {
      unsigned count = 0;
      for (T *p = this->head_; p; p = p->next)
        ++count;
      return count;
    }
    template <class A> T *add(const A &args)
    {
      T *record = loka::core::LokaNew<T>(this->site_, args);
      if (record)
      {
        record->next = this->head_;
        this->head_ = record;
      }
      return record;
    }

  private:
    CardRecords(const CardRecords &);
    CardRecords &operator=(const CardRecords &);
    T *head_;
    const loka::core::LokaAllocationSite site_;
  };

  /** Stable handles registered through the card's constructor declaration doors. */
  struct JsSeatRecord
  {
    enum Kind
    {
      STRING,
      INTEGER,
      BOOLEAN
    };
    struct Initial
    {
      JSContext *context;
      Kind kind;
      Initial(JSContext *c, Kind k)
          : context(c),
            kind(k)
      {
      }
    };
    explicit JsSeatRecord(const Initial &initial)
        : next(0),
          context(initial.context),
          kind(initial.kind),
          value(JS_UNDEFINED)
    {
    }
    ~JsSeatRecord()
    {
      JS_FreeValue(this->context, this->value);
    }
    JsSeatRecord *next;
    JSContext *const context;
    const Kind kind;
    JSValue value;
    loka::app::scene::NodeState<loka::core::String> string;
    loka::app::scene::NodeState<int> integer;
    loka::app::scene::NodeState<bool> boolean;
    loka::app::scene::DerivedNodeState<loka::core::String> formatted;
  };

  class JsCardNode;
  /** A child borrows this record; only its ancestor card owns the JS value. */
  struct JsHandlerRecord
  {
    struct Initial
    {
      JsCardNode *card;
      JSContext *context;
      JSValue value;
      Initial(JsCardNode *n, JSContext *c, JSValueConst v)
          : card(n),
            context(c),
            value(v)
      {
      }
    };
    explicit JsHandlerRecord(const Initial &initial)
        : next(0),
          card(initial.card),
          context(initial.context),
          value(JS_DupValue(initial.context, initial.value))
    {
    }
    ~JsHandlerRecord()
    {
      JS_FreeValue(this->context, this->value);
    }
    JsHandlerRecord *next;
    JsCardNode *const card;
    JSContext *const context;
    const JSValue value;
  };
} // namespace smirkycard
#endif
