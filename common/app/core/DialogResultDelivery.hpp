#ifndef LOKA_DIALOG_RESULT_DELIVERY_HPP
#define LOKA_DIALOG_RESULT_DELIVERY_HPP

class App;

namespace loka
{
  namespace app
  {
    /** Admission-only view of a rail-owned dialog transport. The rail keeps this
        object stable until Window destruction, including across hide/reopen. */
    class DialogResultDelivery
    {
    public:
      /** Opaque borrowed suffix of entries eligible for silent reclamation. */
      class Retirement
      {
      protected:
        Retirement() {}
        ~Retirement() {}
      };

      virtual ~DialogResultDelivery() {}

    private:
      friend class ::App;
      virtual bool hasRunnableWork() const = 0;
      virtual void deliver() = 0;
      virtual Retirement *retirementSnapshot() const = 0;
      virtual void reclaim(Retirement *snapshot) = 0;
    };
  } // namespace app
} // namespace loka

#endif
