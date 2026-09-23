#ifndef LOKA_TESTS_TEXT_EDITOR_SETTLE_AUDIT_HPP
#define LOKA_TESTS_TEXT_EDITOR_SETTLE_AUDIT_HPP
#include <cstdio>
#include "app/scene/state/RequestSettlement.hpp"
#include "testing/scene/ScenarioAudit.hpp"
#include "testing/snap/SnapFormat.hpp"
namespace loka
{
  namespace scenario_tests
  {
    inline const char *SettleStimulusName(app::scene::Settlement stimulus)
    {
      switch (stimulus)
      {
      case app::scene::SETTLE_ATTACH:
        return "attach";
      case app::scene::SETTLE_PROPS:
        return "props";
      case app::scene::SETTLE_INPUT:
        return "input";
      case app::scene::SETTLE_RESTORE:
        return "restore";
      case app::scene::SETTLE_DEFERRED:
        return "deferred";
      }
      return "unknown";
    }
    inline const char *SettleReplyName(app::scene::Reply<app::LineCursor>::Kind kind)
    {
      typedef app::scene::Reply<app::LineCursor> Reply;
      switch (kind)
      {
      case Reply::NO_REPLY:
        return "none";
      case Reply::GRANTED:
        return "granted";
      case Reply::CLAMPED:
        return "clamped";
      case Reply::REFUSED:
        return "refused";
      }
      return "unknown";
    }
    inline std::string SettleCursorText(const app::LineCursor &cursor)
    {
      char text[64];
      std::sprintf(text,
                   "%u:%u:%d",
                   static_cast<unsigned>(cursor.line.generation),
                   static_cast<unsigned>(cursor.line.seq),
                   cursor.column);
      return text;
    }
    /** Portable step grouping through the existing key/value audit surface.
        Cursor identities are generation:sequence:column, never native offsets.
        The caller clears the trace immediately before the step's stimulus. */
    inline bool RecordTextEditorSettleAudit(long step,
                                            const app::testing::SettleTrace<app::LineCursor> &trace,
                                            dsl::testing::ScenarioAuditSink &audit)
    {
      if (trace.overwritten())
        return false;
      dsl::SnapRecord record;
      for (unsigned i = 0; i < trace.size(); ++i)
      {
        const app::testing::SettleTraceRow<app::LineCursor> &row = trace.at(i);
        if (!row.count && row.before == row.after)
          continue;
        char text[128];
        std::sprintf(text, "stimulus=%s takes=%u before=", SettleStimulusName(row.stimulus), row.count);
        std::string value = text + SettleCursorText(row.before) + " after=" + SettleCursorText(row.after);
        for (unsigned take = 0; take < row.count; ++take)
        {
          const app::scene::Reply<app::LineCursor> &reply = row.takes[take];
          std::sprintf(text,
                       " take%u=%s seam=%d reason=%d requested=",
                       take,
                       SettleReplyName(reply.kind()),
                       static_cast<int>(row.seam[take]),
                       static_cast<int>(reply.kind() == app::scene::Reply<app::LineCursor>::REFUSED ? reply.reason()
                                                                                                    : app::EDITOR_OK));
          value += text + SettleCursorText(reply.requested()) + " applied=";
          value +=
              reply.kind() == app::scene::Reply<app::LineCursor>::REFUSED ? "none" : SettleCursorText(reply.applied());
        }
        std::sprintf(text, "settle.%ld.%u", step, i);
        record.set(text, value.c_str());
      }
      return audit.recordVerdict(record);
    }
  } // namespace scenario_tests
} // namespace loka
#endif
