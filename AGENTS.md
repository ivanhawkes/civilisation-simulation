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

The user requests delegation when it reduces expensive primary-agent work or avoids loading
large investigations into the primary context. Follow `docs/agent-workflow.md` when choosing
models and effort for roadmap tasks. Delegate only a bounded, independent task whose result
can be checked cheaply; do small edits and short searches directly.

- Default roadmap implementation: `gpt-6.1-sol`, high effort. Routine follow-up: medium.
  For host/script contracts, fixed-point design and difficult determinism/replay failures,
  prefer `gpt-6-astra`, high; use extra high only for a demonstrated difficult problem.
  Recommend a primary-model change when needed; do not claim to change the active model.
- Use the project agent roles under `.codex/agents/`: `manifest_scout` and
  `manifest_maintainer` (`gpt-6-luna`, medium), `manifest_small_fix`
  (`gpt-6.1-terra`, medium), `manifest_worker` (`gpt-6.1-sol`, medium), and
  `manifest_reviewer` (`gpt-6.1-sol`, high). If named roles are unavailable in the
  current session, explicitly select the corresponding model/effort and pass its scope.
- Start workers with fresh context where supported. Give the specific question, relevant
  file paths, constraints, acceptance criteria and exclusive ownership of any files to edit.
  Ask for concise findings or a patch summary, not raw logs or a repeat of the whole task.
- Use at most two sub-agents concurrently. Workers must not delegate further unless the
  primary explicitly assigns a further delegation. Reuse a worker for related follow-up.
- Only the primary schedules builds and the final gate. The two-job compile limit applies
  across the whole team; workers must not start builds or clang-tidy independently.
- Keep architectural decisions and integration with the primary. Review delegated changes
  and run the relevant checks before accepting them. Escalate an uncertain bounded task
  once rather than repeatedly retrying a cheaper model. Do not promise lower total tokens:
  sub-agents add context and coordination overhead.

## Session context after roadmap tasks

After completing each roadmap task, recommend one of: continue this session, run `/compact`,
or start a fresh session with `/clear` or `/new`. Give a brief reason based on the next task's
dependence on the current discussion and the amount of stale investigation/build output.
Do not compact or clear automatically. Before recommending a fresh session, leave a concise
handoff with the current commit/branch, uncommitted work, decisions, validation and next step.
Distinguish conversation context from the managed KV/prompt cache; do not claim to measure,
flush or compact that cache, invent context-usage figures, or guarantee token/cost savings.
