# Host and script boundary

Roadmap item 1 is defined by this contract. It is a design decision, not an implemented game
host or script ABI. The reference is Jettison `feature-base` at
`f85092027eda49c29001f7589608df1f0b8827fb`, matching
[`source/external/CMakeLists.txt`](../source/external/CMakeLists.txt). Recheck the API inventory
when updating that pin.

## Deployment and ownership

Manifest's authoritative systems run as C++ game code linked into a Jettison game executable
through `Manifest::Game`. A content mod supplies definitions, policy scripts and presentation
assets to that host. Jettison has no native C++ mod ABI: mounting `manifest.pack` cannot register
or execute these C++ modules. The existing sample application needs an explicit Manifest
composition change; there is currently no Manifest game executable or registration entry point.
Keep that composition in compiled game code, using the sample application's startup as the
reference. Do not introduce a shared-library loader or a second Wasm runtime.

| Owner | Responsibilities | Extension surface |
| --- | --- | --- |
| Jettison engine | Fixed steps, command queue, world save/load/hash, reflection, VFS, layered definitions and migrations, utility AI, behaviour trees, script sandbox, rendering, input and audio | Existing SDK APIs below |
| Manifest native game code | Generation, production and consumption, prices, progression eligibility and spending, effects, venues, reservations, matching, settlement, expiry and validation | Reflected definitions, registered components/services, systems, commands and events |
| Manifest policy scripts | Score host-offered trade options; later score eligible upgrades or economic policy options when the host supplies those hooks | Named scalar decision inputs, evaluated inside the step |
| Optional event scripts | Observe committed outcomes, log and keep non-authoritative per-script notes | Reflected events outside the step; initially `log` and optionally `store` |
| Presentation and developer tools | Display snapshots/reasons, collect player commands, console and terminal workflows | Engine presentation/input APIs and the shared console |

The existing `types`, `config`, `universe`, `progression`, `sim`, `ecs`, `effects` and `market`
modules remain native. Their algorithms and authored balance are not ported wholesale to Wasm.
Data determines goods, prerequisites, effect parameters and policy weights; native code enforces
their invariants. Scripts may change a preference, never waive a prerequisite, create resources,
settle a trade or write components directly.

## Reuse the SDK

Paths in this inventory are relative to the selected Jettison checkout, normally `../jettison`.
They identify existing facilities, not Manifest bindings already implemented.

| Facility | Existing API / source | Manifest integration still required |
| --- | --- | --- |
| Game composition | `source/engine/app/src/Application.cpp`, `Trading.cpp`, `Traders.cpp` | Register Manifest's game model during compiled host startup; adapt sample consumers explicitly |
| Authoritative state and scheduling | `source/engine/sim/include/jettison/sim/SimWorld.h`: `Registry`, `RegisterComponent`, `RegisterCommand`, `RegisterSystem`, `AddService`; `Stepper.h` | Port/adapt Manifest state and systems into one world and specify dependencies and turn cadence |
| Persistence and replay | `SimWorld.h`: `Save`, `Load`, `StateHash`, recording/replay; `sim/SaveGame.h`; `serialise/Migration.h` | Register reflected state and migrations; persist Manifest counters, progression, reservations and pending work |
| Content | `vfs/FileSystem.h`; `sim/DefinitionRegistry.h`; `serialise/Archive.h`: `YamlArchive::ReadLayered`; `source/engine/app/src/Mounts.cpp` | Define Manifest schemas, validation and migrations, then replace host-path loading with VFS and layered definitions |
| Decisions and sequences | `ai/UtilityAi.h`: `DecisionDefinition`, `Option`, `Choose`; `ai/BehaviourTree.h`; `source/engine/app/src/Traders.cpp` | Offer valid Manifest options and bounded facts; reuse the trader tasks/trees where their semantics fit |
| Script execution | `script/ScriptHost.h`: `SendCommandsTo`, `LoadAll`, `WithScripts`, `Dispatch`, `Store`, `SetStore` | Register the chosen game surfaces and wire input calls and event dispatch into the host |
| Script authoring | `script/Bindings.h`, `ScriptHost::WriteHeader`, `WriteRustModule`; `source/CMakeLists.txt`: `jettison_add_script` | Generate bindings after game reflection registration; compile and stage the selected scripts |

Abbreviated header paths such as `sim/SaveGame.h` are under
`source/engine/<module>/include/jettison/`; app source filenames share `source/engine/app/src/`.
The engine's
`docs/architecture.md` (simulation, persistence, files and scripts), `docs/scope.md` and
`docs/sample-game.md` explain the intended composition.

The sample's `Jettison::App` trading model is game code, not a general engine economy service.
Its integer credits and crates do not specify Manifest's minor units. Reuse its integration
pattern, flight/docking infrastructure and suitable trader tasks. Manifest's order book replaces
the sample market maker for Manifest venues; both must not settle the same goods. Sample fields
and commands must be adapted to the chosen Manifest units before those consumers share state.

## One authoritative world

The hosted game uses one `Sim::SimWorld` as the owner of simulation state. Register entity state
as reflected components and non-entity authoritative state through a `Sim::WorldService`, whose
save bytes also enter `StateHash`. Progression resources/tiers, economy turn counters, identifier
allocation, stock, purses, orders and reservations all belong in that saved and hashed world.
An opaque `SaveGame` section alone does not put state in the world's hash. Reserve separate
game sections for state outside simulation, including the engine's `Script.Store`.

Today's `Simulation` owns civilisations while `EcsWorld` owns another registry populated by
`MarketplaceService::LoadSimulation`. That is a development workflow, not a live synchronisation
contract. Roadmap item 4 must adapt these algorithms/components to the host registry/services;
do not run two authoritative economies or repeatedly copy balances between them. In particular,
`Simulation::AdvanceTick`'s automatic route trading and the marketplace must not both move the
same inventory. Hosted settlement belongs to the marketplace.

Keep authored IDs and YAML keys unchanged (`ItemCopper`, `SkillMining`, `Civ-01`, and so on).
Definition names remain stable content references. Map game IDs to live host entities explicitly;
a Manifest string ID is not an `entt::entity` bit pattern. Validate liveness and ownership at every
command boundary and restore/rebuild the mapping on load. Scripts receive only reflected values,
never a registry pointer or a reference into native state.

Use the host's fixed step and simulation clock. An economic turn is scheduled by an integer step
counter in the world, never a terminal refresh or event handler. The sample game proposes turns
every 200 steps; item 4 must specify and test the final cadence and system ordering rather than
equating today's development tick with an engine step. Native systems remain responsible for
production, consumption, expiry and settlement even when no scripts are loaded.

## First script surface: trade preference

Use the existing `Ai::DecisionDefinition` and `ScriptHost::WithScripts` path. The first Manifest
policy hook will be `manifest.trade.score`, a dimensionless score in [0, 1] for one feasible
trade option. This is a planned name, not a registered input today. Keep the default preference
in the host/data so a game without the optional script has a defined native policy.

1. Native code enumerates feasible trades from the authoritative state in stable ID order. It
   checks venue access, resources, quantities and cargo constraints; a policy score is no proof
   of validity. Host calculations produce finite, bounded option facts such as margin, demand,
   route cost and competing traders, normalised to documented [0, 1] ranges.
2. Wrap the option's native input callback with `WithScripts`. A script answering
   `manifest.trade.score` may request only that option's named facts through `jettison.input`.
   The native callback accepts an explicit list of fact names and rejects any other name;
   it does not route arbitrary requests into the world or back into scripts.
   Final fact names and normalisation formulas land with their integer model and tests; do not
   publish today's fractional money or quantity fields as those facts.
3. `Ai::Choose` selects among those options using the definition's consideration ranges/curves.
   Equal scores choose the earliest option; all zero scores mean no action. Use a linear [0, 1]
   consideration for this score so a script failure's zero cannot become a positive preference
   through inversion. Keep a native fallback only for an absent optional script, not for a call
   that failed after loading.
4. Native code validates the selected action again and applies it through the same command
   validation as other actors. A decision made inside the step is a deterministic consequence
   of world state; reuse the engine's `SimWorld::Apply` pattern rather than recording it as a
   new external command. AI selection does not bypass affordability or reservation checks.

The engine's scalar input ABI is `double`. Keep it for dimensionless decision values; it is not
a currency/quantity ABI. Raw balances, order amounts, prices, effects affecting those values and
rounding stay in native integer/fixed-point calculations specified by roadmap item 2. Positions
may remain doubles under the engine simulation rules. Do not freeze economic wire layouts,
scales or conversions in this task.

For later upgrade preferences, native code first checks the prerequisite graph, exclusivity,
tier limits and resources, then offers eligible options. Scripts may rank them; the native
progression system buys the chosen tier. Later economy rules likewise answer bounded inputs
inside the step. These hooks need native adapters; the SDK has no Manifest economy or
progression callbacks today. Behaviour sequences continue to use the existing engine trees and
native tasks; there is no script behaviour-task hook in this pin.

## Capabilities, failure and ABI

Script definitions live under the engine's `defs/scripts/` and name VFS `.wasm` paths. Use the
existing definition registry, layering and `jettison_add_script`, initially Rust `no_std` as in
the engine examples (C is also supported). Add these files to Manifest's package in item 5;
the current `manifest/core-config/` documents are not engine script definitions.

- Input calls use fresh instances. They can read only option inputs; sending commands or
  accessing the key/value store while answering an input traps. There is no arbitrary world
  query, host filesystem, wall clock, WASI or libc access. Reflected snapshot reads and automatic
  player identity for handler commands are future engine work, not APIs available at this pin.
- Begin with no `send:*` grants. Policy scripts need no mutation capability; `input` is allowed
  by a definition answering inputs, and `log` can be granted for diagnostics. An optional
  observer may receive `log`/`store`. Store data is versioned by the script and bounded by the
  engine; it must not contain authoritative progression, economic state or decision inputs.
- Future event-driven player actions may use only specific `send:<Command>` grants for commands
  registered on the attached world and marked `Reflect::Attribute::Script`. Logical candidates
  are post order, cancel own order and request upgrade, each validated atomically by native
  code. Their concrete reflected types, units, reasons and controller binding await items 2/4.
  Do not grant setters for balances, stock, tiers or effects. Capability declarations are not
  player authentication; the current host does not inject a controller/sender identity.
- `WithScripts` prefers a script answer over a game input of the same name. Duplicate script
  providers are refused on load, and an input name has no enforced namespace. Reserve the
  `manifest.` prefix by convention. Validate required decision inputs during game startup;
  `Ai::CheckDecision` checks structure, not name resolution, and `LoadAll` logs and skips a
  failed script. An absent optional provider uses the declared native default; a missing
  required provider must fail game setup before stepping.
- Fuel is deterministic instruction metering (definition default 100,000; host ceiling
  10,000,000), with bounded memory (16 pages by default; ceiling 256). Reuse `HostLimits`.
  A trap or non-finite input answer yields zero; an unknown native input requested through a
  script traps if the callback throws. A handler trap resets its instance. The 250 ms watchdog
  default stops a script for the run; this is machine-dependent containment, not a deterministic
  simulation rule. A watchdog stop invalidates a determinism check; report it rather
  than treating such a run as replay evidence.
- Generate C/Rust bindings from game reflection after registering the types. The engine copies
  reflected values and checks Wasm pointers, counts, bools and enumerators. Unsupported fields
  can be omitted (for example `std::optional`); use explicitly supported payload types and
  inspect generated layouts. Never export the current C++ classes, maps or fractional economy
  structs directly. Generated size/offset assertions do not provide a runtime module ABI
  handshake: stage scripts and bindings from the same selected host build.

Event handlers run outside the step when presentation drains events, in script-name order.
Any future granted commands enter the next-step queue and are recorded with player commands;
replay consumes those recorded commands without dispatching the live handlers again. Handlers
are unsuitable for production, turn scheduling or other whole-world rules because their timing
depends on frames. An earlier successful send or store is not rolled back when a handler later
traps: a handler call is not a transaction. Native command validation must preserve invariants
for every individual command.

## Completion and follow-through

Item 1 is complete as a boundary definition: native ownership, the first script decision,
capabilities, lifecycle, failure behaviour and SDK reuse are specified. No runtime bindings or
gameplay claims follow from completing it. The current loaders still take host paths, economic
values still include doubles, and the staged package remains configuration content only.

Implementation acceptance is assigned to the remaining roadmap items:

| Item | Required evidence before claiming integration |
| --- | --- |
| 2: units | Specified scales, checked arithmetic, rounding, reservations and explicit preservation of authored balance; no fractional economic ABI |
| 3: content | Typed VFS/registry loads, layered patches and migrations, stable authored IDs, contextual errors and directory/pack equivalence |
| 4: world | One saved/hashed world; fixed-step ordering and cadence; native command ownership, rejection and atomicity; save continuation and equal replay hashes across Debug/Release/sanitised builds |
| 5: policies and scenario | Generated-binding fixtures; optional-provider fallback and required-provider rejection; valid/forbidden imports, input purity and zero-on-trap; deterministic score/tie behaviour; event-command replay without duplicate handlers; directory and pack run the same engine-hosted scenario |

Existing SDK evidence includes `source/engine/script/tests/ScriptTests.cpp` (capability checks,
fuel, watchdog, fresh inputs, forbidden input effects and invalid answers),
`source/engine/script/tests/BindingsTests.cpp` (layouts and generated bindings) and
`source/engine/app/tests/TradersTests.cpp` (hosted trader decisions). Manifest must add tests for its
adapters when implemented; the current `mod` test proves mounted content equality only.
