#include "StandalonePerformanceTests.hpp"

#include "support/TestVerify.hpp"

#include <climits>
#include <cstdio>
#include <string>

#include "core/String.hpp"
#include "platform/file/FileIO.hpp"
#include "standalone/StandalonePerformance.hpp"
#include "testing/snap/SnapFormat.hpp"

void testStandalonePerformanceSessionSummarizesThreeCompletedRuns()
{
  loka::standalone_tests::StandalonePerformanceSession session(3);
  const bool sessionValid = session.isValid();
  LOKA_VERIFY(sessionValid);
  LOKA_VERIFY(!session.isComplete());

  LOKA_VERIFY(session.recordRun(100, 127));
  LOKA_VERIFY(session.recordRun(200, 241));
  LOKA_VERIFY(session.recordRun(300, 316));
  LOKA_VERIFY(session.isComplete());

  std::string report;
  const bool reportRendered = session.buildReport(report);
  LOKA_VERIFY(reportRendered);
  LOKA_VERIFY(report
              == "performance_version=1\n"
                 "clock=platform_profile_ticks\n"
                 "scope=standalone_flow_application\n"
                 "run_count=3\n"
                 "run.1.elapsed_ticks=27\n"
                 "run.2.elapsed_ticks=41\n"
                 "run.3.elapsed_ticks=16\n"
                 "summary.elapsed_ticks.min=16\n"
                 "summary.elapsed_ticks.max=41\n"
                 "summary.elapsed_ticks.mean=28\n");

  std::printf("testStandalonePerformanceSessionSummarizesThreeCompletedRuns passed\n");
}

void testStandalonePerformanceSessionRefusesInvalidOrIncompleteRuns()
{
  loka::standalone_tests::StandalonePerformanceSession tooFew(2);
  loka::standalone_tests::StandalonePerformanceSession tooMany(11);
  const bool tooFewValid = tooFew.isValid();
  const bool tooManyValid = tooMany.isValid();
  LOKA_VERIFY(!tooFewValid);
  LOKA_VERIFY(!tooManyValid);

  loka::standalone_tests::StandalonePerformanceSession incomplete(3);
  std::string report("preserved");
  LOKA_VERIFY(incomplete.recordRun(10, 20));
  const bool incompleteRendered = incomplete.buildReport(report);
  LOKA_VERIFY(!incompleteRendered);
  LOKA_VERIFY(report == "preserved");
  LOKA_VERIFY(!incomplete.recordRun(40, 30));
  const bool incompleteValid = incomplete.isValid();
  LOKA_VERIFY(!incompleteValid);

  loka::standalone_tests::StandalonePerformanceSession stoppedClock(3);
  LOKA_VERIFY(!stoppedClock.recordRun(50, 50));

  std::printf("testStandalonePerformanceSessionRefusesInvalidOrIncompleteRuns passed\n");
}

void testStandalonePerformanceSessionMeasuresAcrossSignedTickWrap()
{
  loka::standalone_tests::StandalonePerformanceSession session(3);
  LOKA_VERIFY(session.recordRun(LONG_MAX - 2, LONG_MIN + 2));
  LOKA_VERIFY(session.recordRun(10, 20));
  LOKA_VERIFY(session.recordRun(20, 30));

  std::string report;
  LOKA_VERIFY(session.buildReport(report));
  LOKA_VERIFY(report.find("run.1.elapsed_ticks=5\n") != std::string::npos);

  std::printf("testStandalonePerformanceSessionMeasuresAcrossSignedTickWrap passed\n");
}

void testStandaloneScenarioVerdictRetainsFirstTerminalResult()
{
  loka::standalone_tests::StandaloneScenarioVerdict succeeded;
  const bool preflightRefused = succeeded.refusesCompletedPass();
  LOKA_VERIFY(!preflightRefused);
  succeeded.begin();
  const bool incompleteRefused = succeeded.refusesCompletedPass();
  LOKA_VERIFY(incompleteRefused);
  loka::dsl::SnapRecord successRecord;
  successRecord.set("status", loka::dsl::SnapStatusOk());
  succeeded.observe(successRecord);
  loka::dsl::SnapRecord emptyRecord;
  succeeded.observe(emptyRecord);
  const bool succeededRefused = succeeded.refusesCompletedPass();
  LOKA_VERIFY(!succeededRefused);

  loka::standalone_tests::StandaloneScenarioVerdict failed;
  failed.begin();
  loka::dsl::SnapRecord failedRecord;
  failedRecord.set("status", loka::dsl::SnapStatusError());
  failed.observe(failedRecord);
  const bool scenarioRefused = failed.refusesCompletedPass();
  LOKA_VERIFY(scenarioRefused);

  std::printf("testStandaloneScenarioVerdictRetainsFirstTerminalResult passed\n");
}

void testStandalonePerformanceReportWritesCompletedSession()
{
  loka::standalone_tests::StandalonePerformanceSession session(3);
  LOKA_VERIFY(session.recordRun(10, 15));
  LOKA_VERIFY(session.recordRun(20, 27));
  LOKA_VERIFY(session.recordRun(30, 41));

  const char *const reportPath = "standalone-performance-test.txt";
  (void)std::remove(reportPath);
  loka::platform::file::FileHandle reportFile;
  reportFile.displayPath = loka::core::String::Literal(reportPath);
  std::FILE *staleOutput = std::fopen(reportPath, "wb");
  LOKA_VERIFY(staleOutput != 0);
  if (staleOutput)
  {
    LOKA_VERIFY(std::fputs("stale", staleOutput) >= 0);
    LOKA_VERIFY(std::fclose(staleOutput) == 0);
  }
  LOKA_VERIFY(loka::standalone_tests::PrepareStandalonePerformanceReport(reportFile));
  std::FILE *clearedInput = loka::platform::file::OpenRead(reportFile.displayPath);
  LOKA_VERIFY(clearedInput != 0);
  if (clearedInput)
  {
    LOKA_VERIFY(std::fgetc(clearedInput) == EOF);
    LOKA_VERIFY(std::fclose(clearedInput) == 0);
  }
  LOKA_VERIFY(loka::standalone_tests::WriteStandalonePerformanceReport(reportFile, session));

  std::FILE *reportInput = loka::platform::file::OpenRead(reportFile.displayPath);
  LOKA_VERIFY(reportInput != 0);
  std::string report;
  char buffer[256];
  while (reportInput && std::fgets(buffer, sizeof(buffer), reportInput))
  {
    report += buffer;
  }
  if (reportInput)
  {
    LOKA_VERIFY(std::fclose(reportInput) == 0);
  }
  LOKA_VERIFY(report.find("performance_version=1\n") == 0);
  LOKA_VERIFY(report.find("scope=standalone_flow_application\n") != std::string::npos);
  LOKA_VERIFY(report.find("run_count=3\n") != std::string::npos);
  LOKA_VERIFY(report.find("run.1.elapsed_ticks=5\n") != std::string::npos);
  LOKA_VERIFY(report.find("run.2.elapsed_ticks=7\n") != std::string::npos);
  LOKA_VERIFY(report.find("run.3.elapsed_ticks=11\n") != std::string::npos);
  LOKA_VERIFY(report.find("summary.elapsed_ticks.mean=7\n") != std::string::npos);
  LOKA_VERIFY(std::remove(reportPath) == 0);

  std::printf("testStandalonePerformanceReportWritesCompletedSession passed\n");
}
