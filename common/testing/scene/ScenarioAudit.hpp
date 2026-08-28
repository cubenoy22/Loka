#ifndef LOKA_DSL_TESTING_SCENARIO_AUDIT_HPP
#define LOKA_DSL_TESTING_SCENARIO_AUDIT_HPP

#include <cstdio>
#include <string>

#include "dsl/flow/Flow.hpp"
#include "platform/file/FileHandle.hpp"

namespace loka
{
  namespace dsl
  {
    class SnapRecord;

    namespace testing
    {
      enum ScenarioAuditTerminalStatus
      {
        SCENARIO_AUDIT_SUCCEEDED = 0,
        SCENARIO_AUDIT_FAILED,
        SCENARIO_AUDIT_CANCELED
      };

      enum ScenarioAuditErrorKind
      {
        FLOW_ERROR_KIND_SCENARIO_AUDIT = 1004
      };

      enum ScenarioAuditErrorCode
      {
        FLOW_ERROR_SCENARIO_AUDIT_WRITE_FAILED = 1
      };

      /** Immutable fact emitted once when a scheduled scenario step leaves
          pending. The sink must consume the value synchronously. */
      class ScenarioStepTerminal
      {
      public:
        ScenarioStepTerminal(
            int stepId, const char *name, long dueTick, long tick, StepRunStatus status, const FlowError &error);

        int stepId() const;
        const std::string &name() const;
        long dueTick() const;
        long tick() const;
        StepRunStatus status() const;
        const FlowError &error() const;

      private:
        int stepId_;
        std::string name_;
        long dueTick_;
        long tick_;
        StepRunStatus status_;
        FlowError error_;
      };

      /** Immutable first-match selection fact. A selection without an arm
          records the optional-otherwise miss as arm=none. */
      class ScenarioMatchSelection
      {
      public:
        explicit ScenarioMatchSelection(int matchStepId);
        ScenarioMatchSelection(int matchStepId, int armIndex);

        int matchStepId() const;
        bool hasArm() const;
        int armIndex() const;

      private:
        int matchStepId_;
        int armIndex_;
      };

      /** Immutable child-step terminal fact with its owning Match arm. */
      class ScenarioSubstepTerminal
      {
      public:
        ScenarioSubstepTerminal(int matchStepId,
                                int armIndex,
                                int stepId,
                                const char *name,
                                long dueTick,
                                long tick,
                                StepRunStatus status,
                                const FlowError &error);

        int matchStepId() const;
        int armIndex() const;
        int stepId() const;
        const std::string &name() const;
        long dueTick() const;
        long tick() const;
        StepRunStatus status() const;
        const FlowError &error() const;

      private:
        int matchStepId_;
        int armIndex_;
        ScenarioStepTerminal step_;
      };

      /** Borrowed synchronous destination for scenario audit facts. The owner
          must outlive every ScenarioFlow that refers to it. */
      class ScenarioAuditSink
      {
      public:
        virtual ~ScenarioAuditSink() {}
        virtual bool recordStep(const ScenarioStepTerminal &record) = 0;
        virtual bool recordMatch(const ScenarioMatchSelection &record) = 0;
        virtual bool recordSubstep(const ScenarioSubstepTerminal &record) = 0;
        virtual bool recordVerdict(const SnapRecord &record) = 0;
        virtual bool recordTerminal(ScenarioAuditTerminalStatus status) = 0;
      };

      namespace scenario_audit_detail
      {
        /** Owns the shared one-attempt transition used by audit emitters. */
        class OnceEmissionState
        {
        public:
          enum Decision
          {
            ATTEMPT_REQUIRED = 0,
            ALREADY_RECORDED,
            ALREADY_REFUSED
          };

          OnceEmissionState();

          Decision next() const;
          bool settle(bool accepted) const;
          bool isSettled() const;

        private:
          enum State
          {
            STATE_WAITING = 0,
            STATE_RECORDED,
            STATE_REFUSED
          };

          mutable State state_;
        };

        /** Keeps one scheduled step's audit transition coherent across
            repeated ScenarioFlow runs. */
        class StepTerminalEmitter
        {
        public:
          StepTerminalEmitter(ScenarioAuditSink *sink, int stepId, const char *name, long dueTick);

          bool emit(long tick, StepRunStatus status, FlowError &error) const;

        private:
          ScenarioAuditSink *sink_;
          int stepId_;
          std::string name_;
          long dueTick_;
          OnceEmissionState emission_;
        };

        /** Emits one Match arm-selection fact at most once. */
        class MatchSelectionEmitter
        {
        public:
          MatchSelectionEmitter(ScenarioAuditSink *sink, int matchStepId, int armIndex);
          MatchSelectionEmitter(ScenarioAuditSink *sink, int matchStepId);

          bool emit(FlowError &error) const;

        private:
          ScenarioAuditSink *sink_;
          ScenarioMatchSelection record_;
          OnceEmissionState emission_;
        };

        /** Emits one child-step terminal fact at most once. */
        class SubstepTerminalEmitter
        {
        public:
          SubstepTerminalEmitter(ScenarioAuditSink *sink,
                                 int matchStepId,
                                 int armIndex,
                                 int stepId,
                                 const char *name,
                                 long dueTick);

          bool emit(long tick, StepRunStatus status, FlowError &error) const;

        private:
          ScenarioAuditSink *sink_;
          int matchStepId_;
          int armIndex_;
          int stepId_;
          std::string name_;
          long dueTick_;
          OnceEmissionState emission_;
        };

        /** Keeps the scenario's one logical verdict/terminal transition
            coherent across cached result reads and orderly teardown. */
        class TerminalEmitter
        {
        public:
          explicit TerminalEmitter(ScenarioAuditSink *sink);

          bool emit(ScenarioAuditTerminalStatus status, const SnapRecord &record) const;
          bool emit(ScenarioAuditTerminalStatus status) const;
          bool isSettled() const;

        private:
          bool emit(ScenarioAuditTerminalStatus status, const SnapRecord *record) const;

          ScenarioAuditSink *sink_;
          OnceEmissionState emission_;
        };
      } // namespace scenario_audit_detail

      /** Exclusively owns a line-oriented scenario audit file. Each successful
          record crosses the platform durability boundary; fclose is normal
          cleanup only. */
      class ScenarioAuditFile : public ScenarioAuditSink
      {
      public:
        ScenarioAuditFile(const loka::platform::file::FileHandle &file, const char *scenario);
        virtual ~ScenarioAuditFile();

        bool isValid() const;
        virtual bool recordStep(const ScenarioStepTerminal &record);
        virtual bool recordMatch(const ScenarioMatchSelection &record);
        virtual bool recordSubstep(const ScenarioSubstepTerminal &record);
        virtual bool recordVerdict(const SnapRecord &record);
        virtual bool recordTerminal(ScenarioAuditTerminalStatus status);

      private:
        bool writeEscaped(const std::string &value);
        void writeHeader(const char *scenario);
        bool finishRecord(bool written);

        loka::platform::file::FileHandle fileLocation_;
        std::FILE *file_;
        bool healthy_;

        ScenarioAuditFile(const ScenarioAuditFile &);
        ScenarioAuditFile &operator=(const ScenarioAuditFile &);
      };
    } // namespace testing
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_TESTING_SCENARIO_AUDIT_HPP
