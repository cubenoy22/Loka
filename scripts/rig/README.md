# Verification rig tooling

This directory owns the host-side release-preflight orchestrator, its shared
result contract, platform adapters, and tracked rig descriptors.

Use `loka-rig.py` as the public entry point. Files under `macos/` and `toolbox/`
are adapter implementation details; their `local.example.ini` files document
machine-local mappings that must remain untracked when copied and filled in.

Ordinary build helpers remain under `scripts/macos/` and `scripts/mame-*`.
Nothing in this directory changes how a shipping or example application starts.
See [the rig contract](../../docs/LOKA_RIG.md) for usage and lifecycle details.
