# Keyed declaration seats

> **Status:** Normative
> **Owns:** Declaration ownership and replacement sequencing for Keyed
> **Does not own:** API signatures, allocation implementation, or scheduler policy
> **Code truth:** `common/app/nodes/nestable/Keyed.hpp`,
> `common/app/scene/boundary/detail/BranchSeatDeclaration.hpp`,
> `BoundaryNode::replaceSeatBranch`
> **Verification:** Keyed contract pins in `tests/NodeMatchTests.cpp` and the
> MineSweeper scenario and ownership pins

A Keyed definition borrows its enclosing boundary and a member declarer. The
boundary must outlive the definition. Each committed declaration owns its key
snapshot, definitions, and nested seat plans. A fresh candidate owns those same
facts separately until materialization succeeds. Failure preserves the current
branch and key snapshot, so a later external update can retry the current key.

The seat window uses a separate NodeComposition and the current boundary or
Section state owner. It never opens the boundary's declaring window: boundary
composition and bindings persist. Nodes created beneath the candidate still
open their ordinary binding windows at attach. The declarer supplies a root
with `c.declare`; the declaration encloses that definition in a Fragment so a
nested seat can switch its root without changing the outer seat's root identity.

Nested plans belong to their declaration scope. A scoped plan key refers to
that owner, so two declarations can reuse anonymous slots without aliasing.
Runtime rows remain in the existing boundary ledger. Candidate runtime rows
are staged until the outgoing subtree and its nested parked residents have
crossed the detach/retire line. Only then are the new declaration and runtime
rows published. No subtree reconciliation or parking applies to Keyed's own
outgoing branch. Before replacing a committed declaration, the boundary drains
all runtime and parked references to its scope, including nested declaration
scopes. The scope-destruction wall checks that no ledger reference survives;
it remains active in lifecycle-audit builds even with NDEBUG. Plan keys always
carry their real scope; a scope-less key cannot be default-constructed.

Definitions may copy the uncommitted declarer instruction. A committed
Declaration is not cloneable: its plan identities belong to its own scope.
Lifecycle-audit builds reject copying a definition after declaration commit.

Key changes use the normal synchronous boundary UPDATE path. Successful
replacement detaches and retires the outgoing branch through the Match retire
door. Reclamation runs at the next owning clock boundary; the explicit follow-up
flush is silent. No scheduling override or retry flag is introduced.

Observation registration describes the committed tree. An ObservedStatePassScope
begins before traversal and completes on every exit, including a non-nestable
boundary; DETACH is a no-op. Both root and nested boundary paths finish their
compose result before the scope completes. Begin registers the current seat
sources so writes during composition keep their CHILD|LAYOUT classification.
Replacement withdraws outgoing declaration sources before destroying their
scope, then registers committed seat sources and the boundary's own sources.
Seat evaluation precedes the descendant composition walk, so surviving child
nodes subsequently register their own shared-source uses in the same pass.
Newly committed declaration sources register at publication. Completion only
finishes the existing observation pass. A removed declaration's source loses
its observation; a source still used by another node or seat remains registered.
Failed candidates never publish observations. No additional flags, counters,
or per-source reference counts describe the pass.

## Review risk profile

Four flags require explicit review under AGENTS.md: new definition ownership
and cleanup; multiple new borrowed context fields; State/Boundary interaction;
and dirty/attach/detach behavior. This note records the combined design rather
than treating those changes as independent conveniences. Review lenses are
allocation refusal, scope lifetime, observation cancellation, and clock order.
The contract pins cover same-key writes, failure/retry, enclosing Section
ownership, nested seats and parked reentry, a direct nested-seat root, candidate
observation refusal, and removed/shared observations. MineSweeper pins preserve
the click and drain sequence and all board audits.

Validity invariant: a published runtime seat row borrows a live declaration plan
and a runtime parent/state owner that outlive that row. Replacement, outer-arm
retirement, detach, and boundary teardown remove runtime rows before destroying
their definition scope or reclaiming their runtime owner. A failed candidate
publishes neither rows nor observations. Memory reclamation cannot invoke the
declarer or write application State.

The declarer and candidate materialization visit only the seat's new subtree;
retirement visits its old subtree and existing nested parked residents. Existing
boundary routing still visits its seat rows once per UPDATE. Observation pass
completion walks the observed ledger owned by that boundary once per pass,
matching the ownership of the existing begin-pass walk. There is no per-node or
per-key scan of another boundary's ledgers. Successful replacements restore all
committed seat sources in their own boundary, each using the existing linear
observed-entry lookup: K replacements across S seat sources and E observed
entries can cost O(K * S * E). Scope retirement likewise filters the owning
boundary's runtime and parked ledgers per outgoing declaration scope.

## Candidate 68K size evidence

The delegator measured candidate 95e41429 against
`tools/ci/retro68_68k_size_baseline.json` at main 70e85250:
LokaMine68K +6272 bytes (about +5900 from E2), LokaSmirkBench68K +4224
(E2 share about +1280), LokaHello68K +3584 (E2 share about +1150), and the
other four applications +1536..+1920. The MineSweeper E2 attribution was
48% shared seat-scope/context plumbing present in every app, 31%
KeyedDefinition<int>/Declaration/MemberDeclarer, and 20% newly instantiated
NodeState<int> family, minus 664 bytes for removed RecomposingBoundaryFor.
E3 removal of the local-recompose family (about 6.8 KB of symbols measured in
LokaMine68K) is expected to return most of it. This is supplied candidate
evidence, not a fresh measurement of the corrections or a passing size gate.
