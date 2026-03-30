#ifndef LOKA_ATTR_DSL_TESTS_HPP
#define LOKA_ATTR_DSL_TESTS_HPP

void testLokaAttrDslV1Core();
void testPlatformNodeHandlerRegistration();
void testPlatformNodeHandlerReplacement();
void testPlatformNodeHandlerRejectsInvalidTypeKey();
void testPlatformLayoutHandlerRegistration();
void testPlatformLayoutTraversalResultY();
void testPlatformLayoutHandlerReplacement();
void testPlatformLayoutHandlerRejectsInvalidTypeKey();
void testPlatformLayoutHandlerSamePointerReregister();
void testPrepareProjectedLayoutDelegation();
void testProjectedLayoutUsesActiveBoundaryModel();
void testPrepareProjectedLayoutRejectsNullController();
void testContainerLayoutHelpersAdvanceResultY();

#endif // LOKA_ATTR_DSL_TESTS_HPP
