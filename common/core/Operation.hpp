#ifndef LOKA_CORE_OPERATION_HPP
#define LOKA_CORE_OPERATION_HPP

#include <cstddef>

namespace loka
{
  namespace core
  {
    namespace testing { struct OperationTestAccess; }
    class StateTracker;
    class PushStateTracker;

    /** Remaining clock work shared by every ledger in an Operation. */
    struct OperationBudget
    {
      explicit OperationBudget(size_t workRounds = 1000, size_t stateSteps = 1000)
          : rounds(workRounds), stateIterations(stateSteps) {}
      size_t rounds;
      size_t stateIterations;
    };

    enum OperationStatus
    {
      OPERATION_SETTLED,
      OPERATION_JOINED,
      OPERATION_REFUSED_CHAIN_LIMIT,
      OPERATION_REFUSED_STATE_BUDGET
    };

    /** Completed clock result; rounds counts work rounds, never empty probes. */
    struct OperationOutcome
    {
      OperationOutcome(OperationStatus result, size_t workRounds)
          : status(result), rounds(workRounds) {}
      const OperationStatus status;
      const size_t rounds;
    };

    enum OpenResult
    {
      OPEN_OK,
      OPEN_ALREADY_OPEN,
      OPEN_REFUSED_BUSY,
      OPEN_REFUSED_NOT_PUSH
    };

    /** Main-thread stack clock borrowing ledgers until close. Nested clocks join
        the outermost clock, which alone settles and closes all ledgers. Ledgers
        and their registered States must outlive that close. Nested tracker guards
        must end before settlement. Completion turns collect, settle, apply, close,
        then reclaim; collection turns only collect, settle and close. Fair rounds
        visit the ledgers present at each round's start. Production turns do not
        yet open ledgers. */
    class Operation
    {
    public:
      explicit Operation(const OperationBudget &budget = OperationBudget());
      ~Operation();
      /** Opens an idle ledger, or recognizes a ledger already in this clock.
          May be called during work rounds, never during cleanup or after close. */
      OpenResult open(StateTracker *tracker);
      /** Checkpoint with routes and ledger levels retained. The shared budget,
          first refusal and work-round count survive every checkpoint. */
      OperationOutcome settle();
      /** True only while the outermost clock drives or cleans up ledgers. */
      static bool isSettling();
      /** True while any clock is active on the main thread. Rails assert the
          absence of a clock before blocking in the OS; tests observe identity
          through the testing access layer instead. */
      static bool hasActive();
      /** Drives and closes once. The destructor closes if this was not called.
          The first refusal wins; cleanup never invalidates a ledger. */
      OperationOutcome close();

    private:
      Operation(const Operation &);
      Operation &operator=(const Operation &);
      friend class PushStateTracker;
      friend struct testing::OperationTestAccess;

      enum Phase { OPEN, DRIVING, CLOSING, CLOSED };
      static Operation *active_;
      Operation *const outer_;
      PushStateTracker *head_;
      PushStateTracker *tail_;
      OperationBudget budget_;
      Phase phase_;
      OperationStatus status_;
      size_t rounds_;

      bool hasWork() const;
      /** Release-build emergency only: unlink without touching State routes.
          Destruction during a driver callback still violates the lifetime
          contract, including the captured round frontier's lifetime. */
      void withdraw(PushStateTracker *tracker);
    };
  }
}

#endif
