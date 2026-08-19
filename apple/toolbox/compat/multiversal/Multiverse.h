#ifndef LOKA_TOOLBOX_MULTIVERSAL_MULTIVERSE_H
#define LOKA_TOOLBOX_MULTIVERSAL_MULTIVERSE_H

// This directory is enabled only for Retro68's GCC-based toolchain, where
// include_next reaches the real Multiversal umbrella after this wrapper.
// Multiversal declares the global Button() Toolbox trap there; rename it so a
// narrow native type such as FSSpec or Rect can cross an existing public
// header without colliding with Loka's app-facing Button DSL.
#define Button LokaMultiversalButtonTrap
#include_next <Multiverse.h>
#undef Button

#endif
