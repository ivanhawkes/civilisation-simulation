# Scope

Manifest supplies the civilisation, progression and trading model for a Jettison game. The
current scope is the C++ development harness, its terminal interface, authored YAML and mod
content packaging. Jettison owns rendering, input, audio, physics, engine scheduling, VFS,
serialisation and the sandboxed script runtime.

The configuration package mounts as a Jettison mod. It does not yet make the sample game execute
Manifest's economic rules. The [host/script boundary](host-script-boundary.md) defines linked
native ownership of those rules and capability-limited Wasm policy inputs. Implementing the
integer economy, content loaders, world adapters and scripted scenario remains next; see
[roadmap](roadmap.md). A second renderer, engine dependency stack or native shared-library mod
loader does not belong in this repository.
