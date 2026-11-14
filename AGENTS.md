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

## Models and delegation

**OpenAI models are for planning only. They must not implement project work.** This applies
to the primary agent and all sub-agents, regardless of model, effort or role name, and
supersedes earlier recommendations assigning implementation to OpenAI models.

- Planning may include read-only repository investigation, architecture, task breakdowns,
  acceptance criteria, validation plans and handoffs to an implementer.
- Do not use OpenAI models to write or modify implementation code, tests, fixtures, assets,
  build scripts or configuration, or to apply fixes, refactors or mechanical migrations.
- Explicit requests to write planning or workflow documentation permit those documentation
  edits only; they do not authorize implementation.
- Do not assign implementation to the existing OpenAI-backed roles under `.codex/agents/`.
  Their presence and configured defaults do not override this policy. Implementation belongs
  to the user or a non-OpenAI implementer selected by the user; no replacement is assumed.
- Follow `docs/agent-workflow.md`. Delegate only bounded, read-only planning investigations
  when they reduce primary context or expensive-model work. Use fresh context where supported,
  supply relevant paths and a specific question, and request concise findings with references.
- Use at most two sub-agents concurrently. Sub-agents must not delegate further unless the
  primary explicitly assigns further delegation. Do not promise lower total tokens.
- OpenAI planning agents describe the required checks in the handoff; implementation and
  build/gate execution belong to the implementer. The two-job local compile limit and the
  restriction on local clang-tidy still apply to implementation work.

## Session context after roadmap tasks

After completing each roadmap task, recommend one of: continue this session, run `/compact`,
or start a fresh session with `/clear` or `/new`. Give a brief reason based on the next task's
dependence on the current discussion and the amount of stale investigation/build output.
Do not compact or clear automatically. Before recommending a fresh session, leave a concise
handoff with the current commit/branch, uncommitted work, decisions, validation and next step.
Distinguish conversation context from the managed KV/prompt cache; do not claim to measure,
flush or compact that cache, invent context-usage figures, or guarantee token/cost savings.
