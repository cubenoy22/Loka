#ifndef LOKA_CORE_OPERATION_HPP
#define LOKA_CORE_OPERATION_HPP

#include <cstddef>

namespace loka
{
  namespace core
  {
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

    /** Main-thread stack clock borrowing ledgers until close. At most one clock
        is active. Ledgers and their registered States must outlive the clock;
        a window destroys its ledger outside any Operation. Nested tracker
        guards join the clock's level and must end before close.
        Fair rounds visit the ledgers present at each round's start. All routes
        are removed before any refusal cleanup runs. No production entry opens
        this clock yet. */
    class Operation
    {
    public:
      explicit Operation(const OperationBudget &budget = OperationBudget());
      ~Operation();
      /** Opens an idle ledger, or recognizes a ledger already in this clock.
          May be called during work rounds, never during cleanup or after close. */
      OpenResult open(StateTracker *tracker);
      /** Drives and closes once. The destructor closes if this was not called.
          The first refusal wins; cleanup never invalidates a ledger. */
      OperationOutcome close();

    private:
      Operation(const Operation &);
      Operation &operator=(const Operation &);
      friend class PushStateTracker;

      enum Phase { OPEN, DRIVING, CLOSING, CLOSED };
      static Operation *active_;
      PushStateTracker *head_;
      PushStateTracker *tail_;
      OperationBudget budget_;
      Phase phase_;

      bool hasWork() const;
      /** Release-build emergency only: unlink without touching State routes.
          Destruction during a driver callback still violates the lifetime
          contract, including the captured round frontier's lifetime. */
      void withdraw(PushStateTracker *tracker);
    };
  }
}

#endif
