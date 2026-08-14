#include "testing/scene/ScenarioAudit.hpp"

#include <cassert>
#include <cctype>

#include "platform/file/FileIO.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      namespace
      {
        const char *StepStatusName(StepRunStatus status)
        {
          switch (status)
          {
          case FLOW_STEP_PENDING:
            return "pending";
          case FLOW_STEP_SUCCEEDED:
            return "succeeded";
          case FLOW_STEP_FAILED:
            return "failed";
          }
          return "unknown";
        }

        const char *TerminalStatusName(ScenarioAuditTerminalStatus status)
        {
          switch (status)
          {
          case SCENARIO_AUDIT_SUCCEEDED:
            return "succeeded";
          case SCENARIO_AUDIT_FAILED:
            return "failed";
          case SCENARIO_AUDIT_CANCELED:
            return "canceled";
          }
          return "unknown";
        }

        bool IsUnescapedAuditByte(unsigned char value)
        {
          return std::isalnum(value) != 0 || value == '-' || value == '_' || value == '.';
        }
      } // namespace

      ScenarioStepTerminal::ScenarioStepTerminal(
          int stepId, const char *name, long dueTick, long tick, StepRunStatus status, const FlowError &error)
          : stepId_(stepId),
            name_(name ? name : ""),
            dueTick_(dueTick),
            tick_(tick),
            status_(status),
            error_(error)
      {
      }

      int ScenarioStepTerminal::stepId() const
      {
        return this->stepId_;
      }

      const std::string &ScenarioStepTerminal::name() const
      {
        return this->name_;
      }

      long ScenarioStepTerminal::dueTick() const
      {
        return this->dueTick_;
      }

      long ScenarioStepTerminal::tick() const
      {
        return this->tick_;
      }

      StepRunStatus ScenarioStepTerminal::status() const
      {
        return this->status_;
      }

      const FlowError &ScenarioStepTerminal::error() const
      {
        return this->error_;
      }

      scenario_audit_detail::OnceEmissionState::OnceEmissionState()
          : state_(STATE_WAITING)
      {
      }

      scenario_audit_detail::OnceEmissionState::Decision scenario_audit_detail::OnceEmissionState::next() const
      {
        switch (this->state_)
        {
        case STATE_WAITING:
          return ATTEMPT_REQUIRED;
        case STATE_RECORDED:
          return ALREADY_RECORDED;
        case STATE_REFUSED:
          return ALREADY_REFUSED;
        }
        return ALREADY_REFUSED;
      }

      bool scenario_audit_detail::OnceEmissionState::settle(bool accepted) const
      {
        assert(this->state_ == STATE_WAITING && "Audit emission settles once");
        this->state_ = accepted ? STATE_RECORDED : STATE_REFUSED;
        return accepted;
      }

      bool scenario_audit_detail::OnceEmissionState::isSettled() const
      {
        return this->state_ != STATE_WAITING;
      }

      scenario_audit_detail::StepTerminalEmitter::StepTerminalEmitter(ScenarioAuditSink *sink,
                                                                      int stepId,
                                                                      const char *name,
                                                                      long dueTick)
          : sink_(sink),
            stepId_(stepId),
            name_(name ? name : ""),
            dueTick_(dueTick),
            emission_()
      {
      }

      bool scenario_audit_detail::StepTerminalEmitter::emit(long tick, StepRunStatus status, FlowError &error) const
      {
        assert(status != FLOW_STEP_PENDING && "StepTerminalEmitter requires a terminal step");
        const OnceEmissionState::Decision decision = this->emission_.next();
        if (decision == OnceEmissionState::ALREADY_REFUSED)
        {
          error.kind = FLOW_ERROR_KIND_SCENARIO_AUDIT;
          error.code = FLOW_ERROR_SCENARIO_AUDIT_WRITE_FAILED;
          return false;
        }
        if (decision == OnceEmissionState::ALREADY_RECORDED)
        {
          return true;
        }
        if (this->sink_ != 0)
        {
          const ScenarioStepTerminal record(this->stepId_, this->name_.c_str(), this->dueTick_, tick, status, error);
          if (!this->sink_->recordStep(record))
          {
            this->emission_.settle(false);
            error.kind = FLOW_ERROR_KIND_SCENARIO_AUDIT;
            error.code = FLOW_ERROR_SCENARIO_AUDIT_WRITE_FAILED;
            return false;
          }
        }
        return this->emission_.settle(true);
      }

      scenario_audit_detail::TerminalEmitter::TerminalEmitter(ScenarioAuditSink *sink)
          : sink_(sink),
            emission_()
      {
      }

      bool scenario_audit_detail::TerminalEmitter::emit(ScenarioAuditTerminalStatus status) const
      {
        const OnceEmissionState::Decision decision = this->emission_.next();
        if (decision == OnceEmissionState::ALREADY_REFUSED)
        {
          return false;
        }
        if (decision == OnceEmissionState::ALREADY_RECORDED)
        {
          return true;
        }
        if (this->sink_ != 0 && !this->sink_->recordTerminal(status))
        {
          return this->emission_.settle(false);
        }
        return this->emission_.settle(true);
      }

      bool scenario_audit_detail::TerminalEmitter::isSettled() const
      {
        return this->emission_.isSettled();
      }

      ScenarioAuditFile::ScenarioAuditFile(const loka::platform::file::FileHandle &file, const char *scenario)
          : fileLocation_(file),
            file_(loka::platform::file::OpenWriteTruncate(file)),
            healthy_(this->file_ != 0)
      {
        this->writeHeader(scenario);
      }

      void ScenarioAuditFile::writeHeader(const char *scenario)
      {
        bool written = this->healthy_;
        if (written)
        {
          written = std::fputs("loka_scenario_audit version=1 scenario=", this->file_) >= 0;
        }
        if (written)
        {
          written = this->writeEscaped(std::string(scenario ? scenario : ""));
        }
        if (written)
        {
          written = std::fputc('\n', this->file_) != EOF;
        }
        this->finishRecord(written);
      }

      ScenarioAuditFile::~ScenarioAuditFile()
      {
        if (this->file_ != 0)
        {
          std::fclose(this->file_);
          this->file_ = 0;
        }
      }

      bool ScenarioAuditFile::isValid() const
      {
        return this->file_ != 0 && this->healthy_;
      }

      bool ScenarioAuditFile::recordStep(const ScenarioStepTerminal &record)
      {
        if (!this->isValid())
        {
          return false;
        }
        const FlowError &error = record.error();
        bool written = std::fprintf(this->file_,
                                    "step id=%d due_tick=%ld tick=%ld status=%s error_kind=%d error_code=%d name=",
                                    record.stepId(),
                                    record.dueTick(),
                                    record.tick(),
                                    StepStatusName(record.status()),
                                    error.kind,
                                    error.code)
                       >= 0;
        if (written)
        {
          written = this->writeEscaped(record.name());
        }
        if (written)
        {
          written = std::fputc('\n', this->file_) != EOF;
        }
        return this->finishRecord(written);
      }

      bool ScenarioAuditFile::recordTerminal(ScenarioAuditTerminalStatus status)
      {
        if (!this->isValid())
        {
          return false;
        }
        const bool written = std::fprintf(this->file_, "terminal status=%s\n", TerminalStatusName(status)) >= 0;
        return this->finishRecord(written);
      }

      bool ScenarioAuditFile::writeEscaped(const std::string &value)
      {
        if (!this->file_)
        {
          return false;
        }
        for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
        {
          const unsigned char byte = static_cast<unsigned char>(*it);
          if (IsUnescapedAuditByte(byte))
          {
            if (std::fputc(byte, this->file_) == EOF)
            {
              return false;
            }
          }
          else if (std::fprintf(this->file_, "%%%02X", static_cast<unsigned int>(byte)) < 0)
          {
            return false;
          }
        }
        return true;
      }

      bool ScenarioAuditFile::finishRecord(bool written)
      {
        if (!written || !this->file_ || !loka::platform::file::FlushWrite(this->file_, this->fileLocation_))
        {
          this->healthy_ = false;
          return false;
        }
        return true;
      }
    } // namespace testing
  } // namespace dsl
} // namespace loka
