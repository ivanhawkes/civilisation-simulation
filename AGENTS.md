# Manifest

Manifest is a Jettison mod project. Read `docs/coding-standards.md`,
`docs/architecture.md` and `docs/build-and-gates.md` before changing it.

- Jettison's `feature-base` branch is the SDK reference; `main` may lag behind it.
  The sibling checkout is usually `../jettison`. The reproducible SDK commit is in
  `source/external/CMakeLists.txt`; `JETTISON_SOURCE_DIR` is an explicit local override.
- Keep `devenv.nix`, `devenv.yaml` and `devenv.lock` identical to the selected engine SDK.
  Reuse its dependency targets rather than fetching second copies or changing ABI defines.
- Follow the engine's naming, formatting, warnings and simulation rules. Keep authored
  YAML keys and IDs stable when renaming C++ identifiers.
- Build and test with the presets. `./scripts/ci.sh --werror --asan` is the local gate.
- Avoid running clang-tidy locally: the development machine is an underpowered laptop.
  It is opt-in with `--tidy`, enabled in CI. Default local builds to two compile jobs.
- A mounted content package is not yet playable gameplay. Document the host/script boundary
  and the remaining fixed-point/VFS integration accurately; see `docs/roadmap.md`.
