# Next development stages

Approved by the user on 2026-11-15. Implementation belongs to Claude. These stages
refine the [standalone mod plan](standalone-mod-plan.md) and
[gameplay and persistent server plan](gameplay-and-server-plan.md) into the next delivery
sequence: a local, persistent trading game before networking. They are planned work,
not completed capabilities. Each stage should leave a working, testable result and be
split into small commits.

This is the near-term execution order; stage numbers here are distinct from the broader
standalone plan's workstreams. Preserve the product decisions and host/script contract
in the linked documents. Policy scripts and graphical presentation follow the proven
native trading loop.

## Stage 1: Public engine host and standalone bootstrap

Establish how Manifest starts and owns a game through Jettison.

- In Jettison, expose the smallest public lifecycle/composition interface needed for
  game registration, content loading, world creation and shutdown.
- Separate sample gameplay registration from reusable host services.
- In Manifest, add a thin executable and host adapter using public SDK targets.
- Retain pinned CMake consumption and the explicit local checkout override.
- Keep the simulation host independent of rendering and terminal presentation.

Primary areas: Jettison's app lifecycle and public headers, SDK consumer check and
CMake helpers; Manifest's dependency setup and new host/executable targets.

**Exit criterion:** Manifest starts, mounts its package and shuts down cleanly through
public APIs; the Jettison sample still works; a clean checkout builds without the sibling
repository. Test startup failure cleanup as well as successful startup.

Keep this stage focused on lifecycle and mounting. Full definition integration belongs
in stage 2. No sample economy should become active implicitly.

## Stage 2: Validated content and a small scenario

Depends on stage 1. Make packaged content usable by the actual game.

- Adapt configuration and universe loading to the VFS and definition registry.
- Define schema versions, layering rules and contextual validation errors.
- Add a command-line content validator sharing the host's loading rules.
- Author the initial scenario: one system, two civilisations, two venues and eight goods.
- Preserve existing authored IDs and the full content set.
- Establish a content fingerprint for later save compatibility.

Primary areas: Manifest config/universe modules, authored scenario definitions, package
staging and content tests; reusable engine definition APIs only where a gap is demonstrated.

**Exit criterion:** directory and pack inputs produce equivalent validated definitions;
malformed references and incompatible schemas fail clearly; the scenario loads from an
unrelated working directory. This is definition loading, not yet the complete trading loop.

## Stage 3: Company ownership and authoritative commands

Depends on stages 1–2. Establish the state model before connecting the full trading loop.

- Represent companies separately from civilisations.
- Define ownership of treasuries, ships, venue inventory, cargo and orders.
- Register authoritative state in one Jettison simulation world.
- Define command identity, request IDs, execution ordering and structured results.
- Implement ownership, docking, capacity and resource validation.
- Extract command/query services from terminal presentation; retain the existing
  development harness where useful.

Primary areas: Manifest host adapter, ECS, progression and market ownership boundaries,
plus console command services and their test targets. Do not expose trusted development
methods directly as a future remote protocol.

Design save representation and retry semantics here, even though durable recovery arrives
in stage 5. Preserve the established numeric policy and authored identity references.

**Exit criterion:** commands run at fixed-step boundaries; unauthorised or invalid
operations leave state unchanged; company and civilisation funds remain distinct;
command tests run with the TUI disabled.

## Stage 4: Complete the local trading loop

Depends on stage 3. Connect existing algorithms to the authoritative model.

- Integrate production, consumption and marketplace settlement.
- Ensure automatic route trading cannot also transfer the same inventory.
- Implement buy → venue inventory → load → travel → dock → unload → sell.
- Reuse suitable Jettison movement and docking facilities.
- Add one company upgrade, preferably cargo capacity.
- Provide structured queries for balances, inventories, orders and command results.

Primary areas: Manifest simulation, market, effects, progression and host integration,
using public Jettison movement/docking APIs.

Specify economic cadence and system ordering explicitly. Physical transport must have
authoritative location and arrival state; it cannot be a presentation-only animation.

**Exit criterion:** the scenario completes through commands, with partial fills,
cancellation, expiry and rejected actions covered. Goods cannot teleport between venues,
reservations cannot be spent twice, and identical command streams produce identical
state hashes. Verify directory/pack equivalence and cross-build determinism.

## Stage 5: Persistence, recovery and a relocatable release

Depends on stage 4. Turn the working scenario into a world that survives restarts.

- Save all authoritative state, including identity counters, reservations and in-flight travel.
- Add atomic snapshots and journal recovery using Jettison facilities where suitable.
- Persist accepted command ordering before acknowledging durable success.
- Preserve retry detection across restart.
- Validate game/schema/content compatibility before resuming.
- Package a runnable distribution with separate writable user data.
- Resume at the saved simulation step without downtime catch-up.

Primary areas: host persistence/command lifecycle, engine save integration, compatibility
metadata, distribution staging and recovery tests. Apply the detailed acknowledgement,
snapshot-boundary and content-freeze contracts in the gameplay/server plan.

**Exit criterion:** interrupted saves preserve a usable recovery point; durably acknowledged
commands survive a process crash with intact storage; retries cannot duplicate execution;
recovered state matches uninterrupted execution; the distribution runs outside both
checkouts. Backups and restore tests address storage loss separately.

## Validation and delivery

For every stage, Claude records acceptance evidence, updates the roadmap and runs the
relevant suites and required repository gates. Use two local compile jobs; keep local
clang-tidy opt-in. Commit Jettison changes before updating Manifest's immutable SDK pin,
and keep the development environment identical to the selected SDK.

Preserve the standalone plan's CMake consumer matrix, public-API boundaries, directory/pack
checks and relocation checks. Run save/replay and recovery tests as those facilities land.
Do not mark a stage complete based only on configure success or mounted content equality.

After stage 5, the next milestone is authenticated multiplayer: filtered snapshots,
reconnect behaviour, bounded command queues and four-client testing. Policy scripts and
graphical presentation can then build on the proven native loop. The larger benchmark
targets remain provisional; no measured capacity is claimed.

## Handoff

Planning baseline: Manifest branch `feature-scaffold`, commit `f8014e5`, with a clean
working tree before this document was added. No implementation, builds or gates were
performed for this planning update. Recheck both repositories before implementation.

Start with stage 1. At each completed roadmap task, follow the session handoff guidance
in `agent-workflow.md`: recommend continuing, compacting or starting a fresh session,
and leave concrete state and validation evidence when handing off.
