When given a complete, detailed plan, execute it exactly as written.

## Repository and Bake

- `src/` implements the C runtime; `include/siecs.h` is the public API and may be inspected or edited when the task requires it. Each package under `addons/`, each example, and each test project has its own `project.json`. Check that manifest, `Makefile`, and the relevant CI workflow before choosing commands.
- Run Bake from the repository root: `bake build <path>` builds a project, `bake run <path>` builds and runs it, and `bake env` shows the active paths. Use `bake --help` for flags. Building one project does not validate the others.
- After changing internal headers or layouts, use `bake rebuild <path> -r` to avoid stale objects; `-r` rebuilds dependencies. For CI failures, reproduce the failing workflow step with its `--cfg`, compiler, and dependencies. Check that the compiler exists locally before claiming an exact reproduction.
- Execute the C suite with `bake run test` and the C++ suite with `bake run test/cpp`; add `--cfg release` for release validation. A build or a `bake test` heading alone is not proof that tests ran: require exit status and suite PASS/FAIL totals. `make test` is the broader debug/release target; still verify its output contains executed suite results. Bake may print dependency configuration diagnostics even when tests pass.

## Refactoring metrics

Formatting and naming changes are not code reduction.

Refactors must reduce total production code; report missed targets.

Format before refactoring; record production lines excluding blank/comment-only lines. Measure results the same way.

Never reduce lines by removing useful comments/spacing, shortening names, merging statements/declarations, removing braces, or inlining schemas/conditions. Eliminate duplication, branches, dead plumbing, or replace custom code with maintained framework/library features.

## Focused Exploration and Build Order

- Start with `git status --short` and a targeted `rg` search. Read only the affected runtime/API sections, their callers, and the relevant test suite/manifest. Inspect CI workflows when reproducing CI or selecting broader validation, not for every local edit. Avoid dumping entire headers, generated `distr/`, caches, or unrelated addons into context.
- Stabilize and validate one layer at a time: C runtime, C++ interface, then consumers. Finish edits before compiling. Never modify headers or sources while their build runs, or overlap builds that share Bake dependencies/install directories, including builds launched from another repository.
- Prefer the affected target: for example, `bake rebuild test/cpp -r`, then `bake run test/cpp`. Rebuild dependencies after layout changes, and recompile external consumers and standalone binaries against the same headers/libraries. Use `bake env` and resolved paths to confirm the checkout/configuration once rather than repeatedly rediscovering them.
- Do not start with `bake rebuild . -r` or repeat broad clean/rebuild cycles unless the change or a diagnosed artifact mismatch requires them. Compiler contract errors need source fixes; runtime crashes need a backtrace. Neither is evidence of a Bake problem by itself.
- Save verbose output to temporary logs and inspect the first actionable error with a short excerpt. After focused checks pass, run the required broader suites; repeat only for new changes or unresolved failures.
- Once source changes are stable, generate distributions with `make distr`, then run `make check-distr`, `make check-distr-standalone`, and `make check-distr-cpp-standalone`. Record dependency revisions/dirty state before generation; never patch generated files manually. Report local tests, distribution checks, graphical checks, and remote CI separately.

## Tests and public contracts

- Put C tests in `test/src/<suite>.c` and register them in `test/project.json`; put C++ tests in `test/cpp/src/<suite>.cpp` and register them in `test/cpp/project.json`. Unregistered test functions do not run.
- Never edit `test/src/main.c` or `test/cpp/src/main.cpp`: Bake generates them. Confirm each new testcase appears in the executed suite and raises the pass count. For callbacks and events, assert the call count and exact entity/data; include a negative case when relevant.
- Breaking API, ABI, component layout, serialization, and file-format changes are accepted whenever the current design requires them. Implement the new contract directly and update all affected consumers, tests, and documentation; run `bake clean -r` after API/ABI/layout changes.
- Never add or retain legacy support: no compatibility aliases, shims, adapters, deprecated entry points, old-version fallbacks, or migration code for previous APIs, saves, or file formats. Exceptions require an explicit user request.
- Document only the current API and behavior. Do not create migration guides, old-to-new API tables, or documents such as `cpp-migration.mdx`.
