#ifndef LOKA_TOOLBOX_BUILT_IN_SUPPORT_HPP
#define LOKA_TOOLBOX_BUILT_IN_SUPPORT_HPP

class ToolboxScenePlatformController;

/** Returns false if any built-in handler failed to register (the registry
    allocates per handler, and the Classic allocation backend can refuse) —
    that node kind would silently never project for this controller's
    lifetime, so boot asserts on it. */
bool RegisterToolboxBuiltInSupport(ToolboxScenePlatformController &controller);

#endif // LOKA_TOOLBOX_BUILT_IN_SUPPORT_HPP
