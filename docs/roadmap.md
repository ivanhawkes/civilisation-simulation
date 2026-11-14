# Roadmap

The approved [next development stages](development-stages.md) give Claude the near-term
execution order and exit criteria: public host bootstrap, validated scenario content,
company ownership and commands, the local trading loop, then persistence and distribution.
These stages refine the broader integration work below; none is marked implemented yet.

## SDK and build baseline

Completed: consume a pinned `feature-base` Jettison SDK, share dependency targets and development
environment, fix the migrated source layout, apply engine naming and warning conventions, use
doctest, build Debug/Release/sanitised presets, and stage and pack the YAML content as a mod.
The engine RNG and exact state hashes establish a replay check for the development simulation.
The development TUI shares the engine shell's console backend for commands, history, completion
and log capture; its command session is tested independently of terminal rendering.

## Gameplay integration

The [gameplay and persistent server plan](gameplay-and-server-plan.md) fixes the initial
product scope and acceptance scenario: company-owned trading, venue-local inventory,
physical delivery, persistent worlds and server-authoritative commands. Apply these
decisions to the adapters and save schemas below; they remain planned, not implemented.

1. **Defined:** the [host/script boundary](host-script-boundary.md) keeps authoritative economy,
   progression, effects and settlement in linked native game modules. Capability-limited Wasm
   supplies bounded policy inputs, starting with trade preference, and optional event observers.
   Reuse Jettison's world, decisions, reflection and script host; there is no native C++ mod ABI.
   Runtime adapters and an engine-hosted scenario remain in items 2–5 below.
2. **Completed:** [decimal market prices](market-prices.md), with explicit conversion, overflow, rounding
   and reservation tests. Preserve authored IDs, units and prices. Quantities, physics, effect
   stats and other continuous values retain floats/doubles; existing credit ledgers remain
   integers. This supersedes the earlier proposal to convert every economic value to fixed point.
3. Load configuration from Jettison's VFS and definition registry, including layered patches,
   schema versions and migration. Current path-based loaders remain development APIs.
4. Bind progression, economic state and commands to the engine's fixed simulation step, replay,
   serialisation and state hashing. Keep presentation and player input outside simulation code.
5. Add the game's scripts, definitions and presentation assets to the mod staging target. Verify
   an engine-hosted scenario loaded from both a directory and a pack before calling the mod playable.

The existing model and content are recorded in [model reference](model.md).

## Future dedicated server and administration

**Approved direction:** provide a CLI-launched dedicated network server for deployment on a
server platform. The server owns authoritative simulation state and networking, runs without
a renderer or interactive terminal, and supports unattended operation, logs and clean shutdown.
See the [planned architecture](architecture.md#dedicated-server-and-administration-planned).

Keep the current TUI as a development harness while integrating Manifest with Jettison. Preserve
its command workflows and tests as inputs to future administration tooling. A shared,
authenticated administration interface should serve a scriptable CLI, an optional separate TUI
client and potentially Jettison's graphical console. Mutations follow the fixed-step command
path; clients must not maintain a competing authoritative simulation or own the server lifetime.

When the administration interface is designed, assess whether actual operator needs justify
adapting the existing FTXUI presentation. Preserve automated command coverage independently of
the frontend build option. This decision does not schedule immediate TUI removal or expansion;
protocols, hosting platform and deployment tooling remain to be selected.

Follow the [future work sequence](gameplay-and-server-plan.md#sequencing-and-deferred-scope):
first prove the local trading loop, then complete recovery and remote command semantics.
Develop content validation and frontend-independent command coverage alongside integration.
Before public hosting, demonstrate measured capacity, relocation and backup restoration.
The initial target is four connected players; the larger workload is a provisional benchmark,
not a supported-capacity claim.
