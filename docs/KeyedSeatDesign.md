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

The seat window uses a separate NodeComposition and a runtime generation state
owner. States created inside a Keyed/LazyScope runtime generation use the tagged
heap state gate; a Section stays their logical owner; Boundary-lifetime states
and Sections outside such generations keep the StateArena. It never opens the
boundary's declaring window: boundary composition and bindings persist. Nodes created beneath the candidate still
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
NodeState<int> family, minus 664 bytes for the retired boundary declaration adapter.
E3 removal of the local-recompose family (about 6.8 KB of symbols measured in
LokaMine68K) is expected to return most of it. This is supplied candidate
evidence, not a fresh measurement of the corrections or a passing size gate.

## LazyScope

LazyScope is a keyed declaration seat whose arm root is a LazyScopeNode.
Its props are copied values; its key is borrowed live State. The runtime root
is constructed through the ordinary node factory before its member declarer
runs. The root owns generation-scoped state through its concrete inner owner,
without introducing a nested Boundary. Its states use the tagged heap gate,
never the enclosing Boundary's bump-only StateArena: destroying an arena state
does not reclaim its block, so repeated replacements would otherwise accumulate
storage for the Boundary's lifetime. Heap state storage is reclaimed with the
generation through the existing retirement clock.

Constructor state declarations queue registrations; the candidate window
connects them to the root's concrete inner owner before bindings and declareScope.
A refusal of those root-owned registrations records LAZY_SCOPE_STATES_REFUSED
and rejects the candidate,
preserving the committed arm and key snapshot. A descendant Section reports its
allocation refusal to the enclosing Boundary without changing the generation
root's status. No attach-time declaration commits
structure. The completed declaration supplies its prepared root at materialization.
Nested seat plans live in that declaration, and runtime rows live in the enclosing
Boundary. Discovery, source registration, replacement, and scope retirement use
Keyed's existing recursion. Inner tracker commits invalidate the enclosing Boundary.
Children and registrations are released before the inner owner is destroyed.
Each UPDATE visits the scope's seats; tracker begin/end visits its own states.
Each binding window allocates one callback entry per watch, in addition to binding
storage. These costs do not make a scope update constant-time.

## Typed dormant reservation

Keyed requires a `reservation::SeatNodes<List>` argument at both the helper and
its direct definition constructor. `Nodes<T, N, Tail>` describes completed,
accessible, unambiguous Node-derived runtime types and positive counts; `End`
terminates the list. An explicit empty payload is legal. The constructor forces
structural validation even when the descriptor was only named by a typedef.
The header owns the bounded installation capacity and exact diagnostics.

The internal Keyed recipe adds its generation root and the Fragment inserted by
`BranchSeatDeclaration::completeWindow`. Applications describe authored payloads,
including descendants created by their components. Equal target size/alignment
pairs merge with checked addition; this is aggregate layout accounting, not a
concrete-type whitelist. Keyed has one conservative envelope, with no profiles.

At the seat's first declaration attempt, its enclosing Boundary installs an
immutable normalized copy and its checked reservation byte count. No production
NodePartition is created or booted: the table is the only retained allocation.
Invalid emission, normalization or footprint calculation publishes nothing; the
metadata copy retains ordinary nullable allocation handling. Installed facts stay
with that Boundary until reclamation, independently of candidates or replacement
declarations. A missing node envelope can never suppress the live board.
Uncommitted definition clones install distinct tables; no clone borrows another
instruction's mutable installation state. Installation workspace never escapes.

Production allocation still follows the existing routes, with no duplicate
backing reservation. Tests boot their own isolated NodePartition; a synchronous
`NodePartition::buildFixture` operation demonstrates one noncopyable entitlement
through root construction and attach descendants. It rejects over-quota layouts before
placement construction, asserts educationally in debug, and refuses without
fallback in release. A declined factory returns its unconstructed slot; it does
not replenish the ticket. The caller exclusively lends the partition for the
whole operation. This fixture does not enforce production Boundary builds.

Show/Conditional/Match/LazyFlex descriptor doors, bounded reclaim scratch,
production routing, and window resize policy remain later work.
Nested Boundary runtime nodes count in their outer payload; inner residents and
banks belong to the inner Boundary. Repeated nested-landlord workloads remain
outside certification until ancestor-backed provisioning exists. Nested Keyed
instructions install their own metadata; tables from ended nested instruction
lifetimes remain until the enclosing Boundary is reclaimed. Production node
backing and its cleanup are deferred to PR 6.

## Dormant waiting admission

A Keyed reservation now contains one `SeatBuildRequest`. Only the internal
partition fixture enables this path; production still uses its existing node
allocation and replacement route. The request stores demand, not a key value or
candidate root. A committed seat's source only marks demand. A fresh Keyed
Declaration samples the current key when its admitted factory runs, so changes
while waiting coalesce and returning to the retired key constructs a fresh arm.

On destructive retirement the surviving Boundary removes the active child,
retains its logical insertion position in the reservation, cancels descendant
requests and observations before disposing their declaration scopes, and queues
the old root on the existing clock. Canceling demand does not clear the root's
return obligation. The root return includes the partition's registered provider
dependents and complete nested-landlord destruction. Legacy generation snapshots
keep request identities until the whole snapshot has been destroyed. No completion
callback escapes the storage landlord.

At admission the existing scope traversal visits each seat serially. The bank
checks the normalized class-count envelope against its own free lists and the
request's outgoing obligation. Unsupported classes/counts assert and refuse;
insufficient current free counts retain the demand without partial entitlement.
The storage owner constructs a noncopyable `ReturnedSeatStorage` on the stack,
and the synchronous operation holds a `NodeBuildTicket` through its attach walk.
The node-routing connection remains PR 6 work. Parked residents still occupy
slots; changing selection does not make them destructive predecessors.

The existing drain requests structural refresh when surviving demand remains.
It does not call the factory. The following structural traversal can therefore
build without a new State event. Request cancellation withdraws its source and
vacant-position borrows synchronously; the reservation and node-return identity
remain with the Boundary until its ordinary storage cleanup.

Costs are owner-local: notification marks one row; admission walks the bank's
class free lists up to the requested counts; vacant-child edits walk the owning
parent's children and the Boundary's reservation rows; each complete node return
visits those reservation rows. Legacy snapshot completion matches outstanding
requests against that snapshot's node/root identities. This change does not claim
bounded reclaim scratch, non-node storage return, or whole-cycle zero acquisition.
