# Build and gates

Run inside `devenv shell` or the direnv-loaded shell. The development environment and Nix lock
match Jettison. The SDK source pin is in `source/external/CMakeLists.txt`; to test a local engine,
configure with `-DJETTISON_SOURCE_DIR=/path/to/jettison` on `feature-base`.

```bash
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
./scripts/ci.sh --werror --asan --log build/gate.log
```

Presets are `clang-debug`, `clang-release` and `clang-asan`. Each has its own build tree, generated
data and staged mod. The gate runs formatting, Debug build and CTest, Release build and
CTest, and optionally ASan/UBSan build and CTest. `--skip-release` supports a shorter local pass;
CI runs the full gate. `CMAKE_BUILD_PARALLEL_LEVEL`, `CTEST_PARALLEL_LEVEL` and
`MANIFEST_TIDY_JOBS` control concurrency. Clang-format must be version 21.

The CTest suites are `config`, `universe`, `progression`, `sim`, `ecs`, `market`,
`effects`, `data` and `mod`. Module libraries publish their own header sets and dependencies;
integration tests link the aggregate `Manifest::Game`. Existing assertions run under the engine's
doctest version. The simulation suite pins generated and replayed state hashes. The mod suite
mounts the staged directory and generated pack through the engine VFS. The default test build
builds that pack first. The TUI executable is `build/<preset>/source/tools/tui/manifest_tui`.

`manifest_data` merges authored YAML with mikefarah/yq v4 (tested with the devenv version,
4.54.1). Missing yq fails configuration when tests are enabled; with tests disabled the C++
library can build, but packaging still requires yq. Generated YAML lives in the build tree.
Running `bash data/build.sh` manually retains the source-tree `data/core-config` default;
`bash data/build.sh /absolute/output/path` selects an explicit output directory.

`MANIFEST_BUILD_TUI=OFF` omits FTXUI. `BUILD_TESTING=OFF` or `MANIFEST_BUILD_TESTS=OFF`
omits the suites. Manifest tests and the TUI default to off when a host consumes the project.
`MANIFEST_WERROR=ON` makes project warnings fatal. `MANIFEST_CCACHE=OFF` and
`JETTISON_CCACHE=OFF` disable compiler caches. C++ module scanning is disabled because this
project uses headers, keeping compile commands directly usable by clangd and clang-tidy.

The engine is added with `EXCLUDE_FROM_ALL`: configuring loads its complete SDK, but building
Manifest compiles only the engine modules and dependencies it links, plus its packer. Renderer
smoke tests remain Jettison's gate. Do not configure Jettison's unsupported install/export path.

Clang-tidy is opt-in locally (`./scripts/ci.sh --tidy`), since it is expensive on the
development laptop. CI enables it. Local builds default to two compile and test jobs.
