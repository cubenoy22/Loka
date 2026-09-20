#ifndef LOKA_TESTS_ATTRIBUTED_TEXT_TESTS_HPP
#define LOKA_TESTS_ATTRIBUTED_TEXT_TESTS_HPP

void testTextBreakerCharacterization();
void testAttributedTextMetrics();
void testAttributedTextPropsIdentity();
void testAttributedTextDirtySeatsAndEditorPresentation();
void testAttributedTextInvalidProjectionRefusesAndRecovers();
void testAttributedTextHandlerCannotBeReplaced();

void testAttributedTextSegmentationIndependentLayout();
void testAttributedTextWrapUsesJoinedWordsAndRunMetrics();

void testTextBreakerRangesAndRefusal();
void testNullTextShapingDispatch();

void testTextSpanTable();

#endif
