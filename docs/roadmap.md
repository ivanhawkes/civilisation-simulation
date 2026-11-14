# Roadmap

## SDK and build baseline

Completed: consume a pinned `feature-base` Jettison SDK, share dependency targets and development
environment, fix the migrated source layout, apply engine naming and warning conventions, use
doctest, build Debug/Release/sanitised presets, and stage and pack the YAML content as a mod.
The engine RNG and exact state hashes establish a replay check for the development simulation.

## Gameplay integration

1. Define the host boundary: which Manifest systems the Jettison game hosts, and which decisions
   run as capability-limited WebAssembly scripts. The engine currently has no native C++ mod ABI.
2. Convert economic quantities and prices to specified integer minor units, with overflow,
   rounding and reservation tests. Preserve authored game balance explicitly through conversion.
3. Load configuration from Jettison's VFS and definition registry, including layered patches,
   schema versions and migration. Current path-based loaders remain development APIs.
4. Bind progression, economic state and commands to the engine's fixed simulation step, replay,
   serialisation and state hashing. Keep presentation and player input outside simulation code.
5. Add the game's scripts, definitions and presentation assets to the mod staging target. Verify
   an engine-hosted scenario loaded from both a directory and a pack before calling the mod playable.

The existing model and content are recorded in [model reference](model.md).
