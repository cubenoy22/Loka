#ifndef LOKA_TOOLBOX_ACTIVATION_PHASE_HPP
#define LOKA_TOOLBOX_ACTIVATION_PHASE_HPP

/** Which activation the Toolbox run loop is currently servicing. Passed to the
    idle and presentation steps so each one decides for itself what work its
    phase allows, rather than the call site translating "foreground" into a
    per-step boolean. A new phase-dependent behaviour becomes a branch inside
    the step it affects, not another argument threaded from the caller. */
enum ActivationPhase
{
  ACTIVATION_BACKGROUND = 0,
  ACTIVATION_FOREGROUND = 1
};

#endif // LOKA_TOOLBOX_ACTIVATION_PHASE_HPP
