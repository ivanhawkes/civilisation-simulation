# Architecture

Manifest is a Jettison consumer. `source/external/CMakeLists.txt` fetches the pinned engine first,
with its tests, installation and dependency probe disabled. The engine owns all shared dependency
versions and ABI definitions. Manifest reuses `EnTT::EnTT`, `yaml-cpp::yaml-cpp`, and
`doctest::doctest`; it does not redeclare them. FTXUI 7.0.3 is development-only and optional.

The source tree follows the engine's categories. `source/game/` is the counterpart of
Jettison's `source/engine/`: one directory per module, with its own `CMakeLists.txt`,
`include/manifest/<module>/`, `src/`, and unit `tests/` where applicable. Public files are
PascalCase. Developer tools live under `source/tools/`; cross-module suites under `source/test/`.

The modules, in dependency order:

| Module | Owns | Depends on |
| --- | --- | --- |
| `types` | Decimal prices; effect domains, stats, modifiers and operators | Standard library, Jettison Core invariant reporting |
| `config` | Configuration schema, YAML loaders and field readers | Engine yaml-cpp |
| `universe` | Ordered universe name pools | Config loader helpers |
| `progression` | Prerequisites, tiers, costs and exclusive branches | Config |
| `sim` | World generation, production, consumption, pricing and trade routes | Config, universe, Jettison Math |
| `ecs` | Entity registry and plain components | Types, engine EnTT |
| `effects` | Catalogue and ship/civilisation effect systems | Types, config, progression, ECS |
| `market` | Venues, reservations, order matching and settlement | Effects, simulation, ECS, progression |

`cmake/ManifestModule.cmake` mirrors Jettison's module helper: static module libraries, public
header sets, private warnings and implementation includes, and doctest suites. An edge to a module
not yet added fails configuration. `Manifest::Game` aggregates the module targets, analogous to
`Jettison::Engine`; `Manifest::Core` remains a consumer alias. The existing C++ types retain their
`Manifest::Core` namespace, so reorganising implementation modules does not rename symbols again.

The core compiles with `-ffp-contract=off`. The `sim` module uses Jettison's PCG32 generator and
distributions, with a specified Fisher-Yates shuffle instead of `std::shuffle`. Maps and stable IDs
keep simulation iteration ordered. Replay tests pin exact generation and hundred-tick state hashes
across Debug, Release and sanitised builds.

Module unit tests sit beside their implementations. The effects suite exercises the catalogue,
progression, ships, civilisations and markets together, so it is
`source/test/EffectsIntegrationTests.cpp`. `DataTests.cpp`, `ModTests.cpp` and `TestHelpers.h` also
belong in `source/test/`. They do not publish game headers.

`manifest_tui` is a development executable outside the simulation. Its `ManifestConsole` session
registers the development commands with `Jettison::DevTools::Console`, the same backend used by
Jettison's shell. The engine owns tokenisation (including quoted arguments), command dispatch,
help, clear, vars, history, completion, log capture and the bounded 2000-entry scrollback. Manifest
commands validate argument counts and numeric bounds; rejected commands become console errors.
Configuration reloads replace the session together: a failure preserves the previous configuration
and world, while a successful load resets the world and progression before the next generation.

The `manifest_console` tooling library links `Jettison::Devtools`, reusing its Core, Reflect and
ImGui dependencies. The console backend draws nothing; the terminal does not initialise ImGui,
Vulkan or the engine application. `manifest_tui` alone owns FTXUI, wall-clock refresh and the
command transcript. Its sidebar, history navigation and Tab completion read the engine's console
rather than keeping a second command list or history. Build-time paths locate its generated data
independently of the working directory. Console tests drive the session without a terminal.

The mod package is content: `mod.yaml`, `manifest/core-config/` and `manifest/universe/`.
`manifest_mod` stages it and `manifest_pack` uses `jettison_add_pack`. The mod test reads both
forms with `Jettison::Vfs::FileSystem` and compares their contents. These paths are namespaced so
mounting Manifest does not replace the engine sample's definitions accidentally.

The [host/script boundary](host-script-boundary.md) defines the gameplay integration: native
Manifest modules register authoritative state and systems in one Jettison simulation world;
Wasm scripts supply bounded policy inputs through the engine's utility decisions and script host.
The first planned policy scores feasible trades. Event handlers initially observe outcomes only.
This contract does not add runtime bindings or a game executable.

The current C++ loaders still accept host file paths. [Market prices](market-prices.md) use a
decimal representation; quantities, population, effect stats and physics retain floats/doubles,
and existing credit balances and fees remain integers. These are development APIs, not a Jettison
script or save ABI. Engine API conversions, VFS-only content and capability-limited scripts
remain integration work; see the roadmap.

## Dedicated server and administration (planned)

The [gameplay and persistent server plan](gameplay-and-server-plan.md) defines the planned
ownership and recovery contracts. Players control companies distinct from server-controlled
civilisations. Goods reside at venues or aboard ships, with native loading, unloading and
reservation checks. Worlds advance while the server runs and resume without downtime
catch-up. These contracts must guide host integration; the existing development classes
do not yet implement them.

The planned host persists accepted command ordering before acknowledging durable success,
recovers through atomic snapshots and journal replay, and pins world content compatibility.
Clients receive filtered snapshots and use retry-safe commands. See that plan for acceptance
criteria, content validation, operational requirements and deferred scope.

The approved direction is a dedicated network server, launched through a CLI and hosted on a
server platform. It owns the authoritative simulation and networking and runs unattended under
a service manager or container platform, without a renderer or interactive terminal. Startup
uses configuration and command-line arguments; operation provides logs and clean shutdown.
This is a future architecture, not an existing server implementation.

An administration interface will expose authenticated commands and status to a scriptable CLI,
an optional TUI, and potentially Jettison's graphical console. Administrative state changes
must enter the simulation's controlled command path and take effect at the fixed-step boundary.
Clients display server state and submit commands; they do not own a separate authoritative
simulation. Connecting or closing an administration client must not control the server's lifetime.

Keep the current TUI as a development harness during host integration. Its command workflows
and tests are useful inputs to the administration interface, but `ManifestConsole` currently
owns local development state and is not a remote administration client. Consider adapting the
FTXUI presentation into a separate administration client when operator needs justify it.
Preserve command coverage independently of terminal rendering and the FTXUI build option
during that transition; today the console tests are still gated by `MANIFEST_BUILD_TUI`.

Networking and administration protocols, the hosting platform and deployment tooling remain
design decisions. The server must be usable without a TUI; keeping or removing the optional
frontend will depend on operator needs rather than the presence of Jettison's graphical client.
