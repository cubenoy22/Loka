# Loka Rename Plan

Goal: move toward "Loka" branding while keeping the core library stable during refactors.

## Approach
- **App layer first**: UI apps, sample executables, window titles, and user-visible strings can move to "Loka" early.
- **Core library later**: namespaces, internal identifiers, and core files can be migrated gradually alongside refactoring.

## Notes
- Avoid a single big-bang rename for the core; do it incrementally to reduce risk.
- Prefer small, reviewable commits when changing naming in shared libraries.

## Current intent
- Apps can use "Loka" naming immediately.
- Core naming uses "Loka" after the rename.
