# Models and agent workflow

These are project-specific starting choices, based on Manifest's roadmap and the
[OpenAI model guidance](https://developers.openai.com/api/docs/guides/model-selection),
checked on 2026-11-15. The primary agent recommends the model and effort at the start of a
roadmap task. Selecting a different primary model is a client/session setting; an agent
must not claim to have changed its own active model.

## Roadmap tasks

| Work | Lead model and effort | Suitable delegated work |
| --- | --- | --- |
| 1. Host boundary and script capabilities | GPT-6 Astra, high for contract design; GPT-6.1 Sol, high for agreed bindings | Inventory engine APIs, locate existing capability tests, document the agreed contract |
| 2. Integer quantities and fixed-point currency | GPT-6 Astra, high for units, overflow and rounding rules; GPT-6.1 Sol, high for implementation | Inventory fractional fields and authored values; migrate a bounded module after its rules are fixed |
| 3. VFS, layered definitions and schema migration | GPT-6.1 Sol, high; Astra, high if migration semantics remain ambiguous | Find host-path loaders, prepare fixtures, update docs and mechanical call sites |
| 4. Fixed-step commands, saves, replay and hashing | GPT-6.1 Sol, high; Astra, high for ordering/state contracts or difficult replay divergence | Map state ownership and existing tests; implement isolated fixtures against the agreed format |
| 5. Scripts, staging and an engine-hosted scenario | GPT-6.1 Sol, high; Astra, high for capability/ABI design | Packaging inventories, fixture data, scenario documentation and bounded staging changes |

Use medium effort for routine follow-up after the design is settled. Extra high is an
escalation for unresolved arithmetic, contract or determinism problems, not the default for
an entire milestone. Max and ultra are not routine project settings. These recommendations
are engineering judgement, not a measured benchmark of Manifest on each model.

## Worker roles

Project-scoped agent definitions live in `.codex/agents/`; `.codex/config.toml` caps concurrent
sub-agents at two and defaults unspecified workers to Luna with medium effort.

| Role | Model / effort | Scope |
| --- | --- | --- |
| `manifest_scout` | GPT-6 Luna / medium | Inspect code, engine APIs or existing logs; return file references and concise findings; no edits |
| `manifest_maintainer` | GPT-6 Luna / medium | Bounded documentation, fixture and mechanical changes with explicit file ownership |
| `manifest_small_fix` | GPT-6.1 Terra / medium | A small fix with a known reproduction and clear acceptance criteria, especially tooling/UI |
| `manifest_worker` | GPT-6.1 Sol / medium | Implement one defined module or test boundary after its interfaces and invariants are agreed |
| `manifest_reviewer` | GPT-6.1 Sol / high | Inspect a bounded diff for correctness and missing tests; no edits |

All workers inherit the parent's sandbox and approval controls. A role's instruction to avoid
edits is a work constraint, not a separate enforced read-only sandbox. These roles do not
enable Full access or change the existing command exceptions. Restart Codex to discover
new custom agent files; the primary can select the same models explicitly in the current
session.

## Delegation rule

Delegate when a small brief and a concise result cost less primary context or expensive-model
work than the investigation itself. Reading a long failure log or mapping a set of loaders
can qualify; a single search or small edit usually does not. Prefer fresh worker context and
avoid copying the conversation, repeating completed exploration, or asking multiple workers
to do the same job. Use concurrent edits only with disjoint file ownership.

The primary owns design, scheduling, integration and validation. Workers do not start builds
without an explicit allocation from the primary: at most two compile jobs may run across the
whole team, and local clang-tidy stays disabled. Keep successful worker threads available for
related follow-up rather than starting another context unnecessarily.

Delegation can reduce expensive primary-model usage and keep its context focused, but it
often increases aggregate tokens. Model price, total tokens and elapsed time are different
measures. Judge the policy by accepted changes, review/repair work and completed checks;
raise the model or effort when a bounded worker cannot establish the required invariant.
See the [official sub-agent guidance](https://learn.chatgpt.com/docs/agent-configuration/subagents).

## Session context at task completion

End every completed roadmap task with a short context recommendation:

- **Continue** when the next change needs the current reasoning and the session remains focused.
- **Compact** when related work continues but old searches, build output and resolved issues
  dominate the history. In Codex CLI, `/compact` summarises the conversation while preserving
  useful continuity.
- **Fresh session** when the task is complete and the next milestone can start from repository
  documentation plus a short handoff. In Codex CLI, `/clear` or `/new` starts a fresh chat.

Give the reason and advise rather than executing these actions automatically. Before advising
a fresh session, provide the branch/commit, uncommitted changes, important decisions, completed
checks, limitations and the next task so the user can paste the handoff into the new session.
Do not commit changes merely to prepare that handoff.

Conversation compaction is distinct from flushing the server-managed KV/prompt cache. Cache
reuse can reduce input cost, while compaction and a fresh chat may incur summary or context
reconstruction work. Recommend based on continuity and expected future context needs, without
claiming guaranteed savings or inventing unavailable usage metrics. Use context usage from
`/status` if it is available; the user can supply it when the agent cannot inspect it.
See the official [Codex commands](https://learn.chatgpt.com/docs/developer-commands?surface=cli)
and [prompt-caching guidance](https://developers.openai.com/api/docs/guides/prompt-caching).
