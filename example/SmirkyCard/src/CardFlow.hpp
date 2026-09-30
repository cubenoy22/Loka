#ifndef SMIRKYCARD_CARD_FLOW_HPP
#define SMIRKYCARD_CARD_FLOW_HPP
#include "CardRecords.hpp"
#include "CardFlowDescription.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class BindingToken;
    }
  } // namespace app
} // namespace loka

namespace smirkycard
{
  namespace testing
  {
    class CardFlowAccess;
  }

  /** Card-owned synchronous execution and subscription. The description is rooted
      until silent reclamation; Detaching only withdraws the watch. */
  class CardFlow
  {
  public:
    /** Per-card admission phase, with one stack owner for its Running interval. */
    class Admission
    {
    public:
      Admission()
          : active_(0)
      {
      }
      class Execution
      {
      public:
        Execution(Admission &door, CardFlow &flow, bool watching);
        ~Execution();
        bool accepted() const
        {
          return this->door_ != 0;
        }

      private:
        Admission *door_;
        Execution(const Execution &);
        Execution &operator=(const Execution &);
      };

    private:
      CardFlow *active_;
      Admission(const Admission &);
      Admission &operator=(const Admission &);
    };
    struct Initial
    {
      JsCardNode &card;
      JSContext *context;
      JSValue description;
      JSValue handle;
      JsSeatRecord *seat;
      Initial(JsCardNode &c, JSContext *ctx, JSValueConst d, JSValueConst h, JsSeatRecord *s)
          : card(c),
            context(ctx),
            description(d),
            handle(h),
            seat(s)
      {
      }
    };
    explicit CardFlow(const Initial &initial);
    ~CardFlow();
    bool matches(JSValueConst handle) const;
    bool activate(loka::app::scene::BindingToken &token);
    void withdraw();
    bool run(JSValueConst value, bool watching = false);
    void fire();
    /** BindingToken owns its callback; this bridge owns the earlier detach line. */
    void bind(loka::core::StateBase::OnChangeFn callback, void *user, bool immediate);
    void unbind(loka::core::StateBase::OnChangeFn callback, void *user);
    CardFlow *next;

  private:
    friend class testing::CardFlowAccess;
    class Subscription;
    enum CallResult
    {
      Returned,
      Threw,
      Canceled
    };
    CallResult invoke(JSValueConst fn, JSValueConst input, JSValue &output);
    bool live() const;
    JsCardNode &card_;
    JSContext *const context_;
    const JSValue description_;
    const JSValue handle_;
    JsSeatRecord *const seat_;
    Subscription *subscription_;
    CardFlow(const CardFlow &);
    CardFlow &operator=(const CardFlow &);
  };
} // namespace smirkycard
#endif
