# Coding standards

The house rules, taken from the code as it is and from what the work in [history](history.md)
taught. Where today's code disagrees with a rule here, the rule is the target and the code is
fixed when it is next touched.

## Language and toolchain

- C++20, Clang 21, Linux on x86-64.
- Formatting is `std::format`. spdlog is configured to use it rather than its bundled fmt.
- The warnings set on `civsimcore_warnings` is linked privately by every project target and must
  stay clean. `-Werror` is on in the gate. A new warning is fixed, not suppressed. A warning
  that cannot be fixed is removed from the set with the measurement that justified it, as
  `-Wold-style-cast` was.
- clang-tidy uses the curated `.clang-tidy`. A check is added when it is clean over the sources
  or finds something that gets fixed. Exclusions carry their counts.
- clang-format uses the project `.clang-format`: Microsoft base, Allman braces, tabs for
  indentation, 104 columns. The pre-commit hook checks it.

## Naming

| Thing | Style | Example |
|---|---|---|
| Types, enums, enum values | PascalCase | `SceneUpdateSystem`, `State::Running` |
| Functions and methods | PascalCase | `LoadModel`, `Destroy`, `WaitOnWindowResized` |
| Locals and parameters | camelCase | `deltaTime`, `sceneRegistry` |
| Members | `m_` + camelCase | `m_generation` |
| Pointer members (raw and smart) | `m_p` + PascalCase | `m_pDeviceContext` |
| Component fields | camelCase, no prefix | `rotationSpeed`, `active` |
| Macros | `CIVSIMCORECORE_` + upper snake case | `CIVSIMCORECORE_ASSERT` |
| Namespaces | `CIVSIMCORECORE::<Module>` | `CIVSIMCORECORE::Render` |
| Files | PascalCase, one main type per file | `SmokeSequence.h` / `.cpp` |

`Vertex::getBindingDescription` and `m_scene` are known exceptions to fix when touched.

## Headers and includes

- `#pragma once`.
- Public headers: `#include <simcivcore/<module>/Name.h>`. Private headers in the same module use
  quotes.
- Order: the file's own header, third-party, standard library, then engine headers, as
  separate blocks.
- Forward-declare in headers where a pointer or reference is enough; `Application.h` is the
  model.
- Never configure a library with a macro in a header. Configuration goes on the library's CMake
  target. The GLM depth-range bug in [history](history.md) row 7 is why.

## Comments

Comments say why, and what was measured; the code already says what. The reference style is the
existing comments on `Scene::Generation`, `RotationAnimationComponent::angle` and the GLM block
in `source/external/CMakeLists.txt`: they name the hazard, the evidence and the reason for the
shape. A `TODO` names the question it is asking and is resolved where it was asked. No
commented-out code.

## Ownership and lifetime

- `std::unique_ptr` by default, `std::shared_ptr` only when ownership is genuinely shared,
  raw pointers and references for non-owning access.
- Delete copy operations on any type that owns a resource.

## Components and state

- Components are plain structs with default member initialisers and no behaviour. Systems hold
  the behaviour.
- **One rule, one owner.** A constraint that a loader enforces lives in one function that the
  writer also uses. Every format has a round-trip test.

## Errors

- Exceptions for failures at boundaries: start-up, loading, file and format errors. They carry
  enough context to act on: the stage, the path, the version.
- No exceptions inside per-frame or per-step hot paths. Programming errors there are
  assertions.
- Outcomes that are expected and recoverable, such as a lookup that can miss or a command the
  simulation rejects, return `std::optional` or a small result type naming the reason. The
  project is C++20, so there is no `std::expected`, and no stand-in library for it until a hot
  path needs an error payload that a result enum cannot carry.
- `CIVSIMCORE_ASSERT` checks invariants in debug builds; `CIVSIMCORE_VERIFY` checks always.
  Neither has side effects in its condition.
- A stub that cannot do its job fails loudly with `CIVSIMCORE_NOT_IMPLEMENTED()`. It never
  silently succeeds.
- Reports go through `Log.h`: `Log(Severity::Warning, kCategory, "dock {} refused", id)`, with
  one `constexpr Category` per module or subsystem. Not `std::cerr`, and never spdlog directly.

## Dependencies

Every third-party library goes through `source/external/CMakeLists.txt`. It is pinned to a tag
or commit, fetched with `FetchContent`, declared `SYSTEM` so its warnings are not ours, and
given a comment saying why it was chosen over the alternative. A new dependency is judged on
the project's criteria: elegance, efficiency, modern C++ usage, interoperability and
maintenance. It is added to `dependency_probe` with at least one real call into it, and it
follows the integration rules in [build and gates](build-and-gates.md#integration-rules-the-set-follows):
static, one copy of each library, options scoped in a `block()`, and the project's language
features.

## Testing

- Every module has a doctest suite; a phase is not done until its suite covers its exit
  criterion.
- Serialisation has round-trip tests. Simulation has replay tests: the same seed and command
  stream must produce the same state hash.

## Commits

The subject is `Area: what changed, and what it was for`, in the style of the history
(`Scene round trip: the loader refused the file the writer produced`). The body records what
was measured and why the change takes the shape it does. Each commit leaves the gate green.
