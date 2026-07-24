# Origins

Loka looks like an unusual project from the outside: a declarative UI
framework, written in C++98, projecting one application model onto everything
from System 7 to current desktop systems. This document explains where that
shape came from. It did not start as nostalgia, and it did not start as a
framework.

## The first machine

The author's first computer, met at age eight, was a Macintosh Quadra 650
running System 7. Programming began at twelve, on an iBook G3, with
REALbasic and technical books bought before even the English word `Show` —
as in `Window.Show` — meant anything. It is worth pausing on what REALbasic was: an environment
that compiled one project for 68k and PowerPC Macs, and before long for
Windows as well. A first programming tool that quietly demonstrated, to a
twelve-year-old, that software could be written once and outlive any single
platform.

Everything since — AppKit and UIKit years, React, Kotlin, and all the
experiments below — feeds into Loka. So when Loka's support matrix reaches
back to System 7, it is not a compatibility stunt. It is the framework
reaching back to the machine where its author began.

## A research thread, not a product reaction

Beneath everything in this story runs one constant appetite: wanting to write
DSLs — to describe software in a vocabulary of one's own making instead of
assembling it from the platform's objects. UI is where that urge naturally
points, because UI code is where description and machinery fight hardest.

Around 2016 the appetite crystallized into a private research question about
productivity: why does application code *conform to* UI machinery instead of
driving it? On iOS at the time, the normal shape was to place a
`UINavigationController` and let the application organize itself around the
platform's objects. The research inverted that — and the inversion follows
from the DSL urge itself: once a screen is a description, something has to
interpret it, and that something is the model, not the platform. The model
should control the UI, and the platform should follow.

That line predates the mainstream declarative wave. When SwiftUI and the
broader declarative ecosystem arrived, the thread converged with them
independently — declarative was reached as a consequence, not adopted as a
trend. In hindsight the whole chain is short: wanting to write DSLs; writing
UI as a DSL; discovering the model must therefore come first; needing
projection so one description can become real on more than one platform;
Loka. That order still matters: Loka is not "a C++ take on React." It is the
third generation of a line of work that was never about any particular
framework.

## Three generations

**First generation — MusuhiDemo / "Shujaku" (Swift, ~2016).**
A small iOS experiment whose top-level structure was `Library / Service /
State` — no `ViewControllers` directory, no MVC vocabulary. State as a
first-class architectural layer, years before `@State` existed. Six commits,
now archived. The seed pointed in the right direction.

**Second generation — musuhi-concept (React + TypeScript).**
Applications described as YAML data and executed dynamically. Its vocabulary
is revealing in hindsight: `DynamicComponentDefinition` versus
`DynamicComponentInstance` (the definition/instance split that survives in
Loka's `NodeDefinition` versus materialized nodes), and `DynamicEndpoint` /
`DynamicEndpointConnection` (connections between components as first-class,
inspectable objects rather than ambient data flow — the ancestor of Loka's
explicit state binding). It also carried the deeper ambition: software
described as data stays *soft* — changeable without rebuilding.

Development paused there for the most ordinary reason — time, not doubt — and
both repositories were archived only once Loka had taken over the line. In
hindsight, though, the generation order carried a lesson: a dynamic
description layer is at its strongest when a trustworthy core sits beneath
it — real answers about ownership, lifetime, and platform integration — and a
garbage-collected, browser-hosted runtime makes it easy to defer exactly
those questions. Loka chose to answer them first.

Between the generations, the thread surfaced once more in a third language: a
2022 article on [mass-producing similar form screens with a Kotlin DSL on
Jetpack Compose](https://qiita.com/cubenoy22/items/f7fcc832d097ffe1eabf) — a
`PageScope` builder declaring pages of elements, with validation kept reactive
by passing functions where others would hardcode booleans. Same instinct,
fourth host to come: declare the page, let the model drive the rest.

**Third generation — Loka.**
The trigger was almost accidental. Through 2024 the author's workplace began
encouraging AI-assisted development, and by the spring of 2025 it was time to
build something with an AI pair. The starting point was that 2022 article:
could the same DSL idea be rebuilt in Win32 and C++, as a learning exercise?
It was meant to be a small toy. One early choice changed its trajectory —
targeting C++98, so that the toy might also run on Snow Leopard, or perhaps
even Mac OS 9. The working method became a loop that predated today's tooling
for it: explore whether a chaining DSL was even expressible in C++98 in
conversation with one model, solidify each design decision into markdown
specifications, hand those to another model to test implementability, and
iterate — a hand-rolled plan mode.

What came back was unsettling in the best way. Very early it was clear that
this was turning into something far more general than a form-screen
exercise — promising enough to be quietly frightening. And for a while the
framework was a purely theoretical existence: aimed at Win32, but unable to
draw a single control, alive only as unit tests — `State` binding, deferred
notification, tracker semantics — passing on a machine with no GUI attached.
The core was proven as logic before it ever touched a screen. That birth
order left a permanent mark: to this day the scene runtime runs headless on
Linux, and the platform layers are thin projections over a core that never
needed them in order to exist.

The turn from theory to running software came about five months in, when a
second coding agent joined the loop. The design felt right, but where to
begin executing it was genuinely unclear, and as summer 2025 turned to
autumn, so did the mood: without something running on Win32, the project
risked staying beautiful and imaginary. The request to the new agent was
blunt — just make it run. What came back was the first real validation of
the core: surprisingly little glue code was needed, the work never collapsed
into vibe coding, and the first application — a small BMI calculator that
still lives in today's HelloWorld example — drew its first pixels on a
Snapdragon X Elite laptop running Windows 11 on ARM. A framework whose
longest ambition points at System 7 came alive on one of the newest
architectures Windows supports. It has been traveling backwards in time ever
since.

The other platforms fell faster than expected. Win32 matured through the
autumn; Cocoa support began almost on a whim over the 2025–26 New Year
holidays and landed with surprising ease. Then came the harder test: Classic
Mac OS Toolbox. The API had been designed with only Win32 in mind — and its
author's home instincts are AppKit and UIKit ones; look closely at Loka's
`Button` and an `NSButton` shines through — yet the same declarative model
projected onto a 1991-era toolbox remarkably smoothly, with only redraw
behavior putting up a real fight. That was a second validation, stronger
than the first: a platform the model had never been designed for, absorbed
without bending the model. It also supplied a rite of passage — somewhere in
the middle of Classic-side optimization, repeated instability led to
installing MacsBug for the first time in the author's life, decades after
everyone else did.

The toy refused to stay a toy. The 2016 question, the earlier generations'
lessons, and years of platform knowledge had been waiting for exactly this
vessel.

The order was inverted this time: build the hard, explicit core first. C++98 with no
exceptions and no RTTI makes ownership and lifetime questions impossible to
defer — a constraint the second generation's runtime never imposed. Platform
projection replaces the browser, so the same description reaches native
controls on every supported era. The softness ambition is not abandoned; it
returns on top of the solid core as data- and script-driven definitions (see
"Software Should Stay Soft" in [PHILOSOPHY.md](../PHILOSOPHY.md)), now with an
ownership model strong enough to host it safely.

The names trace the same arc. *Musuhi* (産霊) is the generative, binding
force in Japanese mythology — fitting for a study of state and connection.
*Loka* is Sanskrit for *world* — the study grew into a vessel meant to hold
many worlds: platforms, eras, and domains.

## Scale as a consequence, not an ambition

The unusual reach — System 7 through modern macOS, Windows XP-class systems
through ARM64, with animation, video, game, and plugin-editor directions on
the roadmap — is not a feature checklist. It follows from the original
question. If the model truly drives the UI, then the model must not depend on
any particular platform's shape; and the only honest proof of that claim is to
project the same declaration onto platforms separated by thirty-five years.
The retro targets are not the goal. They are the *evidence*, and they double
as design pressure that keeps every abstraction small, explicit, and
inspectable.

They are also, it should be said, loved. The affection is real — for the
machines themselves, for a PowerBook coming alive on a café table, for the
small thrill of a fresh build starting up on hardware from another century.
The distinction this document insists on is narrower than it may sound:
affection supplies the energy, but it is never accepted as the argument.
Every retro target earns its row in the support matrix as proof and as
design pressure. That it is also a joy is what makes the work sustainable.

The same reasoning extends to distribution: the intended long-term shape is
era-correct packaging for each shelf — fat binaries in `.sit` archives for
Classic Mac OS (resource forks intact), Universal Binary archives spanning the
PowerPC and Intel eras, Universal Binary 2 for current macOS, and parallel
Windows tiers from XP-class systems to ARM64 — all built from one source tree.
A release page where those artifacts sit side by side says more about software
longevity than any manifesto.

Decades of platform knowledge — how Toolbox controls dispose, how Win32
repaints, how Cocoa retains — stop being obsolete trivia in this model. Each
technique is encoded once into a projection layer and keeps earning for every
application built afterwards. The platform layers are, deliberately, a museum
that still works.

## Where it is heading

The near-term roadmap ([ROADMAP.md](../ROADMAP.md)) is intentionally ordinary:
tighten lifecycle and ownership contracts, grow the component set, prove the
model with real applications. The applications planned for that proof — an
open device catalog, native editors for hardware synthesizers whose front
panels never matched the depth of their parameter spaces — are chosen so that
each one forces a missing part of the framework into existence.

The long-term direction is the one this document started with: software whose
meaning outlives its environment, and software that stays soft enough to keep
changing. Everything else is implementation.
