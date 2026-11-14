# Scope

Manifest supplies the civilisation, progression and trading model for a Jettison game. The
current scope is the C++ development harness, its terminal interface, authored YAML and mod
content packaging. Jettison owns rendering, input, audio, physics, engine scheduling, VFS,
serialisation and the sandboxed script runtime.

The configuration package mounts as a Jettison mod. It does not yet make the sample game execute
Manifest's economic rules. The [host/script boundary](host-script-boundary.md) defines linked
native ownership of those rules and capability-limited Wasm policy inputs. Decimal market prices
are implemented. Content loaders, world adapters and the scripted scenario remain next; see
[roadmap](roadmap.md). A second renderer, engine dependency stack or native shared-library mod
loader does not belong in this repository.

The approved future scope includes a CLI-launched dedicated network server for unattended
hosting, with authoritative simulation state and a shared administration interface. Keep the
TUI as a development harness for now; a separate TUI administration client is optional and
will depend on operator needs. The server must operate without it. This remains planned work;
see [architecture](architecture.md#dedicated-server-and-administration-planned) and
[roadmap](roadmap.md#future-dedicated-server-and-administration).
