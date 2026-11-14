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
| `types` | Effect domains, stats, modifiers and operators | Standard library |
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

`manifest_tui` is a development executable outside the simulation. It alone owns FTXUI, wall-clock
refresh and session transcripts. Build-time paths locate its generated data independently of the
working directory.

The mod package is content: `mod.yaml`, `manifest/core-config/` and `manifest/universe/`.
`manifest_mod` stages it and `manifest_pack` uses `jettison_add_pack`. The mod test reads both
forms with `Jettison::Vfs::FileSystem` and compares their contents. These paths are namespaced so
mounting Manifest does not replace the engine sample's definitions accidentally.

The current C++ loaders still accept host file paths and the economy still models fractional
quantities and prices with doubles. They are development APIs, not a Jettison script or save ABI.
The engine's integer quantity, fixed-point currency, VFS-only content and script capability rules
apply when that integration is implemented; see the roadmap for the remaining work.
