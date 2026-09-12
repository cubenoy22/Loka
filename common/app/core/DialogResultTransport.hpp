#ifndef LOKA_DIALOG_RESULT_TRANSPORT_HPP
#define LOKA_DIALOG_RESULT_TRANSPORT_HPP

#include "app/OpenFileDialog.hpp"
#include "app/core/DialogResultDelivery.hpp"

class Window;
namespace loka
{
  namespace app
  {
    namespace testing
    {
      class DialogResultTestAccess;
    }

    /** Window-owned dialog envelopes. Only App admission invokes their bindings.
        Context registrations own permission; native wakes own no storage. */
    class DialogResultTransport : public DialogResultDelivery
    {
    public:
      class Registration;
      class ReturnPort;

    private:
      struct Entry;
      /** Intrusive ownership: entries know their chain and unlink in constant time. */
      struct Chain
      {
        Chain()
            : head(0)
        {
        }
        Entry *head;

      private:
        Chain(const Chain &);
        Chain &operator=(const Chain &);
      };

    public:
      /** One context-owned operation, deleted on detach, retirement or retarget. */
      class Registration
      {
      public:
        ~Registration();
        bool matches(const OpenFileDialogProps &props) const;

      private:
        explicit Registration(const OpenFileDialogProps &props);
        OpenFileDialogProps binding_;
        Entry *entry_;
        friend class DialogResultTransport;
        friend class ReturnPort;
        Registration(const Registration &);
        Registration &operator=(const Registration &);
      };

      /** Stack-owned, revocable native return route; never borrows a context. */
      class ReturnPort
      {
      public:
        explicit ReturnPort(Registration *registration);
        ~ReturnPort();
        /** Seals once and returns the still-enrolled Window for a pointer-free wake. */
        Window *seal(const FileChooserResult &result);

      private:
        Entry *entry_;
        friend class DialogResultTransport;
        ReturnPort(const ReturnPort &);
        ReturnPort &operator=(const ReturnPort &);
      };

      DialogResultTransport();
      virtual ~DialogResultTransport();
      void open(Window &window);
      void close();
      /** Refuses closed enrollment or a result channel without its owner tracker.
          The declaring scope must enclose the context and every invocation. */
      Registration *reserve(const OpenFileDialogProps &props);
      virtual bool hasRunnableWork() const;

    private:
      struct Entry : Retirement
      {
        explicit Entry(DialogResultTransport &owner);
        DialogResultTransport &transport;
        Registration *registration;
        ReturnPort *returnPort;
        FileChooserResult result;
        Chain *chain;
        Entry *next;
        Entry **previous;
        void unlink();
        void moveTo(Chain &destination);
      };
      /** Bounded invocation; token release and active retirement precede return. */
      class Invocation
      {
      public:
        explicit Invocation(Entry &entry);
        ~Invocation();
        void deliver();

      private:
        Entry &entry_;
        void *emitterToken_;
        Invocation(const Invocation &);
        Invocation &operator=(const Invocation &);
      };
      static void revoke(Entry &entry);
      void cancel(Entry &entry);
      virtual void deliver();
      virtual Retirement *retirementSnapshot() const;
      virtual void reclaim(Retirement *snapshot);

      Window *window_;
      Chain reserved_;
      Chain pending_;
      Chain active_;
      Chain retired_;
      friend class testing::DialogResultTestAccess;
      DialogResultTransport(const DialogResultTransport &);
      DialogResultTransport &operator=(const DialogResultTransport &);
    };
  } // namespace app
} // namespace loka
#endif
