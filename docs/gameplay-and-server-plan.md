# Gameplay and persistent server plan

Recorded on 2026-11-15 at the user's request after resolving the future-planning
questions. These are the agreed planning baseline, not implemented capabilities.
Implementation is assigned to Claude. This document extends the
[standalone mod plan](standalone-mod-plan.md); it does not replace its SDK integration
stages or the [host/script boundary](host-script-boundary.md).

## Product decisions

| Question | Decision | Implementation consequence |
| --- | --- | --- |
| First playable loop | Operate a trading company: buy at one venue, transport goods, sell elsewhere and reinvest. | Connect the economy to ships, docking, cargo and company progression. |
| Persistence | Worlds persist between sessions and advance while the server runs. No catch-up for server downtime initially. | Resume the saved authoritative step; player disconnects do not stop the world. |
| Player control | One company per player, initially with one ship, a treasury, cargo and orders. Civilisations are server-controlled. | Company ownership is distinct from civilisation identity and finances. |
| Initial scope | One server, one star system, two civilisations, two venues, eight goods and up to four connected players. | Prove the complete loop before expanding the universe. |
| Capacity target | Benchmark 16 companies, 16 venues and 10,000 open orders before expanding public hosting. | These are provisional validation targets, not demonstrated capacity or release promises. |

The first integration scenario runs locally without a renderer, using commands and
automated tests. The multiplayer milestone adds authenticated connections and snapshots.
Graphical presentation uses the same command/query interface.

## Ownership and physical trading

Companies own ships, money, cargo, venue-local inventory and orders. Civilisations own
their populations, production and institutions. Do not expose a civilisation treasury
as a player's wallet. Preserve existing authored civilisation IDs and define separate
company identities and ownership references.

Purchasing goods transfers them into company-owned inventory at the selling venue.
Loading requires a docked ship and available capacity. Unloading transfers cargo into
inventory at the destination venue. Sell orders reserve goods already present at that
venue. A global stockpile must not permit instantaneous transport between venues.

Players and AI use the same asset ownership and native validation rules. Civilisation
production and consumption must integrate with local inventory explicitly. Decide the
concrete component/service representation during host integration; retain one authoritative
world and one settlement owner as required by the host/script boundary.

Start progression with one company purchase that increases cargo capacity or trading
capability. Civilisation progression remains separate. The initial scenario does not
require every authored skill or effect to become playable.

## Commands, identity and queries

The existing development APIs assume trusted local callers. For example, an order
request carries a civilisation ID and tick, and cancellation accepts an order ID without
a caller identity. Do not expose those signatures directly as a remote authority boundary.

Define a host command contract with:

- Authenticated actor identity resolved by the host, never trusted from a claimed owner field.
- A request identifier and persisted retry-detection semantics.
- Validated target identities and payload.
- Server-assigned execution step and deterministic ordering.
- Structured acceptance, rejection and completion results.

Authentication and permissions are host concerns; ownership, docking, affordability,
inventory, reservations and progression eligibility remain native simulation validation.
Administrative mutations use the controlled command path with explicit permissions.
Define stale-command handling and the scope/lifetime of retry identifiers before publishing
a protocol. A retried committed request must not execute twice after reconnect or restart.

Queries return structured, access-filtered snapshots and reasons. Frontends own text and
visual formatting. Clients submit intentions and never advance an authoritative economy.
Begin with full snapshots at a bounded frequency; introduce deltas only when measurements
justify them. Transport, encoding and authentication mechanism remain design choices for
the networking milestone, after the command contract works locally.

Acceptance includes another player being unable to cancel an order, predictable stale
request rejection, retry safety and replay of the exact accepted command ordering.

## Persistence, recovery and content compatibility

Distinguish command receipt from durable commitment. Persist accepted commands with their
execution step and ordering before reporting durable success. Use periodic atomic snapshots
and a command journal to recover from the last consistent state. Snapshot boundaries must
identify precisely which journal entries are already included so recovery never applies
an order or refund twice.

Target no loss of durably acknowledged commands after a process crash, assuming intact
storage. Backups cover storage loss separately. Snapshot frequency, journal format,
durability batching and retention are implementation decisions to measure and document;
they must preserve the acknowledgement contract.

Persist all authoritative state, including outstanding reservations, orders, company and
civilisation progression, inventory location, economic counters and ID allocation. Restart
must not reuse order identities. Save/load continuation and replay must match the world hash.

Freeze content for a running world. Record engine/game versions, schema versions and a
content fingerprint with saves. Upgrades require validation, a backup and a controlled
restart. Specify supported migrations and rollback limits; do not silently run an old
world against changed balance data. No live balance reload initially.

Test interrupted snapshot writes, recovery around journal/snapshot boundaries, retries
after restart, incompatible content and recovery from a backup. A corrupt new snapshot
must not destroy the previous usable snapshot.

The market's current 64-entry fill history serves presentation. Design operational audit
history separately, with explicit retention, rather than treating that buffer as a complete
record or keeping unbounded diagnostic data in authoritative state.

## Content authoring and validation

Add a command-line validator using the same VFS loaders and semantic rules as the host.
It must validate a directory or pack without launching the game and run during packaging
and CI. Report the virtual path, definition ID and responsible source/override for failures.

Cover duplicate IDs, unresolved references, progression graph errors, unsupported schemas,
invalid numeric values and semantic effect combinations, and missing required scripts or
assets. Explain effective definitions after layering so an author can identify which
package supplied an override. Reuse shared validation rather than duplicating loader rules.

Keep authored IDs and YAML keys stable. Retain the established decimal per-unit price
policy and floating quantity types; this plan does not authorise a numeric rewrite.

## Tooling and administration

Extract host-facing command/query services from terminal presentation. Today
`ManifestConsole` owns its own simulation and progression, and its tests are gated by
the TUI option. Keep it as a development harness during migration, then adapt its command
workflows to the host services as appropriate.

Command coverage must run with `MANIFEST_BUILD_TUI=OFF`. A scriptable CLI is the initial
administration frontend; a separate TUI client is optional. Disconnecting any client must
not affect server lifetime or create a competing simulation. Do not expose the unrestricted
development console directly as a remote administration protocol.

## Scale, stability and operations

Create small, expected and stress workloads with explicit company/civilisation, venue,
good and open-order counts. Measure simulation-step duration, command backlog, matching
and expiry costs, memory growth, save size and long-run economy behaviour. Run replay
checks over substantially longer sequences than the current hundred-tick baseline.

Marketplace operations currently gather and sort active orders repeatedly. Measure those
paths before adding indexes or caches. Introduce parallel simulation work only with a
demonstrated need and a preserved deterministic ordering contract.

Long-run invariant checks must account explicitly for production, consumption, fees and
other designed sources/sinks. Do not assert total resource conservation where gameplay
intentionally creates or consumes resources. Verify reservation accounting and rejection
atomicity independently of balance tuning.

Before external hosting, provide a true renderer-free build, graceful shutdown, readiness
reporting, structured logs, bounded command queues and backup/restore procedures. Diagnostics
identify engine/game/content versions. Test release archives outside both source checkouts
and validate save compatibility before deploying an upgrade. Hosting platform, performance
budgets and transport selection remain to be settled with implementation evidence.

## Initial acceptance scenario

1. Start a world with two venues having complementary production and demand, using the
   small eight-good content set.
2. Create a company with one ship and a bounded starting balance.
3. Buy goods, load them, travel, dock, unload and sell; verify ownership, local inventory,
   cargo capacity, reservations and settlement at each transition.
4. Purchase one company upgrade using the proceeds.
5. Disconnect and reconnect without changing ownership or duplicating requests.
6. Restart the server and recover the same committed state without downtime catch-up.
7. Replay recorded commands and obtain the same state hash.

Implement steps 1–4 and local save/replay evidence first. Add actual connection/reconnection
coverage with the networking milestone. Directory and pack content must produce equivalent
results. A content-mount test alone does not satisfy the scenario.

## Sequencing and deferred scope

The approved [next development stages](development-stages.md) expand this sequence into
five bounded deliveries with dependencies and exit criteria. Use that document to schedule
implementation while retaining the ownership and persistence contracts defined here.

| Milestone | Deliverable |
| --- | --- |
| Next | Public Jettison host integration and the local trading scenario. |
| Then | Complete authoritative commands, ownership, saved state and recovery. Define their contracts during the preceding integration. |
| Alongside integration | Content validator and frontend-independent command tests. |
| Before multiplayer | Renderer-free server, identity/permissions, retry semantics and filtered snapshots. |
| Before public hosting | Measured capacity, operational diagnostics, compatible release packages and restore testing. |

Defer interstellar travel, player-controlled civilisations, combat, piracy, insurance claims
and offline catch-up from the initial playable scope. Existing definitions may remain without
making every associated mechanic a release requirement. This deferral does not remove
existing fee/insurance calculations from the model or authorise rebalancing them silently.

## Handoff and validation status

Baseline: Manifest `feature-scaffold` at `9f17d34`; the selected Jettison SDK is
`f85092027eda49c29001f7589608df1f0b8827fb`. Recheck working trees before implementation.
The review inspected documentation, public gameplay interfaces, console ownership, tests
and CI read-only. No builds, benchmarks, recovery tests or capacity measurements were run.

Claude should apply this document when designing the host adapter, company/venue inventory
model, command payloads and persistence schemas. Follow the standalone plan's validation
requirements, two-job local compile limit and opt-in local clang-tidy policy. Record actual
completion in the roadmap only when the relevant acceptance evidence exists.
