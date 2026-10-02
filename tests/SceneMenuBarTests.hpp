#ifndef LOKA_TESTS_SCENE_MENU_BAR_HPP
#define LOKA_TESTS_SCENE_MENU_BAR_HPP

void testSceneMenuBarPublishedWithRoot();
void testSceneMenuBarCloneRefusalKeepsOldScene();
void testSceneMenuBarSecondDeclarationRefused();
void testSceneMenuBarFromChildBoundaryRefused();
void testSceneMenuBarOneComposePerMount();
void testMenuAttachmentDisconnectsOnDetachBeforeRootTeardown();
void testMenuAttachmentDispatchAfterSceneReplacement();

#endif // LOKA_TESTS_SCENE_MENU_BAR_HPP
