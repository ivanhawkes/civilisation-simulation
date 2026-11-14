# Manifest

A civilisation and space-trading simulation for [Jettison](https://github.com/ivanhawkes/jettison).
Worlds produce and consume goods, scarcity changes prices, and progression changes the ships,
markets and civilisations that trade them.

Manifest follows Jettison's `feature-base` SDK, pinned to commit
`f85092027eda49c29001f7589608df1f0b8827fb` (engine 0.1.0). `main` is not the SDK reference.
`devenv.nix`, `devenv.yaml` and `devenv.lock` match that checkout. Shared libraries such as EnTT,
yaml-cpp and doctest come from the engine's targets, including their compile definitions.

## Build

Use the locked devenv shell (`direnv allow`, or `devenv shell`). It supplies Clang 21, CMake 3.28+
and Ninja, along with Jettison's complete SDK. From the repository root:

```bash
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
build/clang-debug/source/tools/tui/manifest_tui
```

For engine development, point at the sibling `feature-base` checkout instead of downloading the
pinned commit:

```bash
cmake --preset clang-debug -DJETTISON_SOURCE_DIR=/home/ivan/projects/jettison
```

This explicit override tests that checkout as it is; it does not verify it against the pin.
Use an ignored `CMakeUserPresets.json` for machine-specific paths. Release and sanitised presets
are `clang-release` and `clang-asan`. `-DMANIFEST_BUILD_TUI=OFF` omits the development UI and its
FTXUI dependency. `-DBUILD_TESTING=OFF` omits tests. The engine's tests and renderer executables
are excluded from the default build.

The complete gate is:

```bash
./scripts/ci.sh --werror --asan --log build/gate.log
```

It checks formatting and builds and tests Debug, Release, and a build with AddressSanitizer
and UndefinedBehaviorSanitizer. CI uses the same locked Nix environment. See [build and gates](docs/build-and-gates.md).

## Development console

The development TUI uses Jettison's shell console backend. It loads the generated configuration
on startup; try `generate 8`, `list`, `tick`, `prices` and `plan`. `help` lists the commands and
`help generate` describes one. Quote paths or civilisation names containing spaces, for example
`show "Abigail Prime"`. Tab completes command names, Up/Down recalls history, PageUp/PageDown
scrolls and End follows new output. `clear` clears the view; the command transcript remains in
`build/tui-session.log` when launched from a working tree with a `build/` directory. `quit`,
`exit` or Ctrl-D ends the session.

## Mod output

```bash
cmake --build --preset clang-debug --target manifest_pack
```

This stages `build/clang-debug/mods/manifest/` and writes `build/clang-debug/manifest.pack` with
Jettison's packer. Put either the directory or the pack under Jettison's user `mods/` directory.
Both contain `mod.yaml`, `manifest/core-config/*.yaml`, and the universe name pools. The mod
integration test mounts both through Jettison's VFS and compares every file.

The package currently contains Manifest's configuration data. Jettison can mount it, but its
sample game does not consume Manifest's configuration schema or execute this repository's C++
economy. The C++ implementation builds as static host-side modules aggregated by `Manifest::Game`
(with `Manifest::Core` as a compatibility alias); the TUI exercises them during development.
The [host/script boundary](docs/host-script-boundary.md) assigns authoritative rules to linked
native game code and bounded policy decisions to Wasm. Market prices now use an exact decimal
representation; other continuous values retain floats/doubles. Shipping gameplay still needs
the VFS loaders, world adapters and scripted scenario in the
[roadmap](docs/roadmap.md).

## Structure

- `source/game/<module>/`: game modules, each with `CMakeLists.txt`, `include/manifest/<module>/`, `src/`, and unit `tests/` where applicable.
- `source/tools/tui/`: the development console session, FTXUI frontend and console tests.
- `source/test/`: cross-module effects, data and mod-pack suites, plus shared test helpers.
- `source/external/`: the pinned engine SDK and optional development-only dependencies.
- `cmake/ManifestModule.cmake`: module target, header, dependency and test rules matching Jettison's module helper.
- `data/`: authored YAML; CMake merges it into `build/<preset>/data/core-config/` with mikefarah/yq v4.
- `mods/manifest/`: mod metadata; generated data is staged alongside it in the build tree.

The modules are `types`, `config`, `universe`, `progression`, `sim`, `ecs`, `effects`, and
`market`. Each has a `Manifest::<Module>` target. `Manifest::Game` links the complete model;
`Manifest::Core` remains an alias for existing consumers. The current C++ API retains its
`Manifest::Core` namespace.

[Architecture](docs/architecture.md), [coding standards](docs/coding-standards.md),
[scope](docs/scope.md), and [model reference](docs/model.md) describe the implementation.

MIT licensed; see [LICENSE](LICENSE).

Clang-tidy is opt-in locally (`./scripts/ci.sh --tidy`), since it is expensive on the
development laptop. CI enables it. Local builds default to two compile and test jobs.
