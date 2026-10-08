#ifndef LOKA_SIMPLE_TEXT_TESTS_HPP
#define LOKA_SIMPLE_TEXT_TESTS_HPP
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(LOKA_RETRO68)
void testSimpleTextEditorReceivesRemainingWindow();
void testSimpleTextOpenSaveAndSaveAs();
void testSimpleTextNewAndSaveWithoutDestination();
void testSimpleTextPendingCommandsAndTerminalResults();
void testSimpleTextReadAndWriteFailuresPreserveDestination();
void testSimpleTextCommitExhaustionPreservesDestination();
void testSimpleTextRepeatedOpenAndCaretReplacement();
void testSimpleTextMenuAndDialogProps();
void testSimpleTextDetachWithdrawsFlows();
#endif
#endif
