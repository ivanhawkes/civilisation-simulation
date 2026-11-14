# Standalone Manifest mod: implementation handoff

Approved direction: 2026-11-15. Implementation is assigned to Claude by the user.
This document records the plan; it does not claim that the integration exists.

The subsequent [gameplay and persistent server plan](gameplay-and-server-plan.md) resolves
the product questions and extends this handoff with company ownership, venue-local inventory,
the first trading scenario, command identity/retry semantics and persistence/recovery rules.
Apply it before implementing host adapters, command payloads or save schemas. Its initial
scope and acceptance scenario govern the gameplay delivered through the stages below.

## Outcome and scope

Manifest remains a separate repository that consumes Jettison through CMake. It builds
its own application, links its native gameplay against the engine and ships its own
definitions, scripts and assets. A standalone distribution runs without a Jettison
checkout or a separately installed Jettison game.

The deliverables are a compiled Manifest host and its content package. Mounting a pack
cannot register Manifest's C++ systems in an arbitrary engine executable. This plan
does not introduce a native shared-library mod ABI or port the economy wholesale to Wasm.

Changes may be made in both repositories. Jettison owns reusable engine services and
extension points; Manifest owns its gameplay, composition and distribution. There must
be no dependency from Jettison to Manifest.

Read both repositories' AGENTS.md and applicable coding/build documentation before
implementation. Preserve the decisions in [host/script boundary](host-script-boundary.md),
[market prices](market-prices.md) and [architecture](architecture.md). Resolve any newly
discovered API constraints explicitly rather than silently changing those contracts.

## Inspected baseline

- Manifest: branch `feature-scaffold`, commit `ad46cd09e080bcf148aef0b433b677a1bb9c18eb`.
- Jettison: sibling `../jettison`, branch `feature-base`, commit
  `f85092027eda49c29001f7589608df1f0b8827fb`.
- Both working trees were clean during inspection. No builds or gates were run for this plan.
- Manifest already fetches that exact SDK commit in `source/external/CMakeLists.txt`,
  supports `JETTISON_SOURCE_DIR`, and reuses engine dependency targets.
- The repositories' `devenv.nix`, `devenv.yaml` and `devenv.lock` match.
- `Manifest::Game` aggregates native game modules. `manifest_mod` and `manifest_pack`
  stage configuration content; the mod test checks mounted content equality.
- Manifest's loaders still accept host paths. There is no Manifest game executable or
  registration entry point.
- Jettison's public `RunRenderApplication` starts its existing application shell.
  `Application::InitResources` and `InitScene` directly load/register sample gameplay.
- Jettison supports source consumption. Its install/export path is explicitly unsupported;
  transitive dependency exports are incomplete.
- Jettison currently requires Vulkan during configuration and configures presentation
  modules even when a consumer only builds selected simulation targets.

Recheck branch state and these assumptions before editing. Do not overwrite unrelated work.

## 1. Formalise the CMake SDK contract

Keep pinned `FetchContent` as the default import mechanism, with an explicit local SDK
override for work across repositories. Do not switch to a moving branch or silently
prefer the sibling checkout. Installed `find_package(Jettison CONFIG REQUIRED)` support
is a separate future milestone if a prebuilt SDK becomes necessary.

In Jettison:

- Document the supported public module targets, build options and consumer helpers.
- Strengthen `source/test/consumer` to exercise the public API and helpers required by
  a game. Keep engine tests, samples and development tools out of a consumer's default
  build unless explicitly requested; retain access to the packer and required resources.
- Check that helpers work from the consumer's directory scope and do not assume that
  the top-level source/build directory belongs to Jettison.

In Manifest:

- Retain the existing dependency ownership and pin in `source/external/CMakeLists.txt`.
- When a parent has already supplied Jettison, validate the complete required target/helper
  contract instead of treating the presence of `Jettison::Math` alone as sufficient.
- Preserve optional tests/TUI and private warning policy. Keep all generated data in
  the build tree and expose packaging without requiring the TUI.

Acceptance: a fresh Manifest checkout can configure/build with the pinned engine and
no sibling checkout; the explicit local override works; parent-project consumption
works without duplicate dependencies; tests/TUI can be disabled while packaging works.

## 2. Expose game composition and add a minimal Manifest host

This is the first runtime milestone. Establish the integration seam before migrating
the full economy or building presentation.

Jettison starting points:

- `source/engine/app/include/jettison/app/RenderApplication.h`
- `source/engine/app/src/Application.h` and `Application.cpp`
- `source/engine/app/src/Mounts.h` and `Mounts.cpp`
- `source/engine/app/CMakeLists.txt` and `source/samples/jetsam/`

Introduce a small public composition contract for registering reflected game types,
loading definitions, attaching world services/systems/commands, creating or restoring
game state and shutting down. Specify ordering and lifetime explicitly. Register types
before loading their definitions; attach required services before creating/restoring
state; load scripts only after their required registrations are available. Specify
failure cleanup and ownership of each object passed across the boundary.

Move sample gameplay registration behind a separate sample composition. Keep the sample
working, but allow a consumer to select its own composition without sample trading or
settlement becoming active. Expose reusable mount facilities through a public API where
needed; do not make consumers include private application headers.

In Manifest, add a host adapter module and a thin executable in the existing source
categories. Names such as `Manifest::Host` and `manifest` are proposed, not existing APIs.
The adapter links `Manifest::Game` and the required public engine modules. Keep game
registration usable without a renderer or terminal. The first host may load definitions
and run a small controlled scenario before full gameplay is integrated.

Acceptance: a Manifest-owned executable loads its content using public SDK APIs;
registration/lifecycle failures are reported; sample gameplay is not implicitly active;
the Jettison sample still passes its relevant checks.

## 3. Load Manifest definitions through the VFS

Primary Manifest areas: `source/game/config/`, `source/game/universe/`, relevant data
consumers, `source/test/ModTests.cpp`, `mods/manifest/` and staging in `CMakeLists.txt`.

Separate parsing/validation from host file access. Make hosted loads use Jettison's VFS
and definition registry. Keep path-based development entry points only as adapters where
useful. Preserve authored IDs, YAML keys and namespaced virtual paths.

Define and document schema versions, supported migrations, override/patch precedence,
missing-reference handling and validation errors. Distinguish VFS file replacement from
registry-level definition layering. Failed loads must not leave partially installed
authoritative definitions. Do not require unpacking a pack into temporary host files.

Acceptance: directory and pack inputs produce equivalent typed definitions; layered
patches and migrations have tests; malformed content, unsupported versions and broken
references report the virtual path and relevant definition; loads work independently
of the process working directory.

## 4. Bind gameplay to one authoritative simulation world

Primary Manifest areas: `sim`, `ecs`, `progression`, `effects`, `market` and the new host
adapter. Reuse Jettison's simulation, reflection, command, save and hash APIs. Extend
the engine only for demonstrated general-purpose gaps.

- Use one `Sim::SimWorld`. Move authoritative entity state into registered components
  and non-entity state into saved/hashed world services.
- Replace the development workflow of copying simulation state into a second registry
  with explicit host ownership. Define stable authored-ID to live-entity mappings and
  restore/rebuild them on load.
- Specify fixed-step system ordering and the integer step cadence of economic turns.
  Keep terminal refresh and presentation outside authoritative scheduling.
- Make marketplace settlement the single owner of hosted transfers. Prevent automatic
  route trading from also moving the same goods.
- Route external mutations through validated commands. Specify rejection reasons,
  ownership checks and atomicity for orders, cancellation, progression and settlement.
- Include counters, ID allocation, stock, purses, reservations, orders and progression
  state in saves and world hashes. Define save versioning and compatibility.
- Preserve decimal per-unit prices and the established quantity/ledger types. Make
  conversions at engine boundaries explicit; do not impose an unapproved numeric rewrite.

Acceptance: the engine advances a minimal production/consumption/trade scenario;
rejected commands preserve invariants; save/load continuation equals uninterrupted
execution; replay hashes match across Debug, Release and sanitised builds. There is
no competing sample economy or second authoritative Manifest registry.

## 5. Produce a relocatable standalone distribution

In Jettison, make required runtime assets and packaging helpers available independently
of sample executables. Separate generic engine resources from sample-game content as
needed. In Manifest, stage the application, required resources, Manifest pack, licences
and runtime dependencies into a distribution directory or archive.

Define startup asset discovery relative to the installed application or an explicit
configuration option, with a separate writable user-data location. Keep development
overrides available. Do not rely on source-tree or build-tree absolute paths.

Require the application's matching Manifest content. The current `mod.yaml` engine
field is a minimum-version check, not proof of a compatible native Manifest host.
Define game/content schema compatibility and script-binding provenance separately.
Missing or incompatible mandatory content must fail startup with an actionable error.

Acceptance: move the staged distribution outside both checkouts and run it from an
unrelated working directory. Verify directory and pack modes, missing-content failures,
writable saves/settings and the documented platform runtime requirements. Players do
not need CMake, yq, source trees or a separate Jettison game installation.

## 6. Add bounded policy scripts and minimal presentation

Follow the existing host/script contract. Start with the planned `manifest.trade.score`
policy: native code enumerates feasible options, scripts rank bounded facts, and native
code validates and settles the result. Reuse the engine runtime, fuel/memory limits,
binding generation and `jettison_add_script`.

Stage definitions, generated-compatible Wasm and required presentation assets. Keep
native fallback for an absent optional provider; reject missing required providers.
Test trap/invalid-score behaviour and deterministic tie handling. Scripts do not gain
direct access to balances, registries or settlement. If event-driven commands are added,
verify replay consumes recorded commands without invoking live handlers a second time.

Add only the presentation needed to exercise and observe the hosted scenario initially.
Keep the development TUI while it remains useful; preserve command tests independently
of whether its frontend is enabled.

Acceptance: directory and pack variants run the same hosted scenario; script capability,
failure and binding checks pass. Describe the result as playable only once a usable
gameplay flow is demonstrated, not merely because mounting succeeds.

## Dedicated-server constraint

The approved architecture includes a CLI-launched, unattended dedicated network server.
Keep composition and authoritative simulation independent of rendering now. For a true
server build, add explicit engine build options and conditional dependency discovery so
configuration does not require Vulkan/windowing/presentation dependencies. An Xvfb render
smoke test is not evidence of a renderer-free server.

Networking, authentication, replication, administration protocols and deployment platform
remain separate design work. A scriptable administration CLI and optional TUI must submit
commands to the server, not own another simulation or control its lifetime. This integration
plan does not claim to deliver those undecided network features.

## Delivery and validation

Implement the stages in order as reviewable commits, subdividing large stages as needed.
For engine API changes, first preserve Jettison's own sample and test behaviour, then test
Manifest against the local override. Commit the engine changes before updating Manifest
to their immutable SDK revision. Update all three devenv files together with the pin when
the selected SDK changes them; do not create independent dependency versions or ABI flags.

The implementer must run the applicable checks; none were executed during planning:

1. External-consumer checks for pinned fetch, local override and parent-provided SDK.
2. Relevant unit/integration suites for each stage, including TUI-disabled coverage.
3. Directory/pack equivalence, schema failure and relocated-distribution checks.
4. Command invariants, save continuation and cross-build replay checks.
5. Manifest's presets and `./scripts/ci.sh --werror --asan`.
6. Jettison's applicable presets/gate, sample/render regression checks when affected,
   and its required review workflow, subject to the user's model/delegation policy.

Default local compilation to two jobs. Avoid local clang-tidy; it remains opt-in locally
and enabled in CI. Do not treat a successful configure or content-mount test as runtime
integration evidence. Record failures and limitations accurately.

Update architecture, build instructions and roadmap as each stage actually lands. Do
not mark future server/networking work complete as a consequence of host integration.

At each completed roadmap task, recommend continuing, compacting or starting a fresh
session according to `docs/agent-workflow.md`. For a fresh session, leave branch/commit,
uncommitted work, decisions, validation and the next concrete step.

## First task for Claude

Recheck the baseline, then implement stage 1 and design the smallest public composition
contract for stage 2 against the actual engine lifecycle. The first runtime deliverable
is a Manifest-owned host loading its own pack through public Jettison APIs. Keep the
full economy migration behind that proven seam.
