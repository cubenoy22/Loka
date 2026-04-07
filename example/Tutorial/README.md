# Loka Tutorial

This example is meant to be explained step by step on video or followed in the
editor.

## Flow

Start with `DoItYourselfNode` in `src/MyAppConfig.hpp`.

When you want to check an answer, switch the active typedef to one of:

- `Step1Node`: a minimal `Hello, Loka`
- `Step2Node`: increment button and counter state
- `Step3Node`: conditional content with `Show(condition) << ...`
- `Step4Node`: predeclared repeated children revealed one by one

## Suggested walkthrough

1. Build and run `LokaTutorial`.
2. Edit `src/DoItYourselfNode.hpp` to recreate the steps yourself.
3. Uncomment a `StepNNode` typedef in `src/MyAppConfig.hpp` when you want to compare.

## Targets

- macOS: `LokaTutorialMacOS`
- Win32: `LokaTutorialWin32`
- Retro68 68K: `LokaTutorial68K`
- Retro68 PPC: `LokaTutorialPPC`
