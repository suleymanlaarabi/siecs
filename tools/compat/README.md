# Flecs Script compatibility tools

This is Phase 0 infrastructure. The SiECS runtime is not implemented. The
candidate endpoint deliberately returns `TODO` with exit status 2 for every
valid request. It does not evaluate scripts, call Flecs, or claim compatibility.
No test status in the upstream backlog is automatically promoted by these tools.

Freeze and inspect upstream outside the repository:

```sh
git clone https://github.com/SanderMertens/flecs /tmp/flecs-script-reference
git -C /tmp/flecs-script-reference checkout --detach fa788c60c87b4effc1124027db86ef0f30a778d4
python3 tools/sync_flecs_script_tests.py /tmp/flecs-script-reference
python3 tools/sync_flecs_script_tests.py /tmp/flecs-script-reference --check
python3 tools/compat/build_oracle.py /tmp/flecs-script-reference
python3 tools/compat/test_phase0.py
```

The Linux oracle build uses upstream CMake sources, a static library, and `CC`
(default `cc`). It installs nothing into Bake and never links SiECS. Build
artifacts live under `.cache/flecs-script-oracle/`. The checkout must be clean
and pinned; generation reads immutable git objects even if the working tree
has changed. The oracle build records its revision and compiler in provenance.

Each runner reads one JSON object from stdin and emits one JSON object:

```sh
echo '{"source":"Root { Child {} }", "mode":"run", "events":[]}' | tools/compat/flecs_script_oracle
echo '{"source":"Root { Child {} }", "mode":"run", "events":[]}' | python3 tools/compat/compare.py
```

The comparison exits 0 for equal complete results, 1 for a mismatch/runner
error, and 2 for a compatibility gap. Its expected current result is 2.
`ir` and `skip_unknown` are optional booleans. The oracle currently supports
`run` and `parse`; managed/reactive/reload requests and mutation events return
an explicit gap. These capabilities must be added before differential coverage
can be claimed for those modes.

The initial snapshot includes newly created named entities, hierarchy, tags,
component values and pairs (including payloads). It retains Flecs reflection
metadata rather than silently deleting host-specific state. Future native
backend normalization must explicitly account for SiECS's distinct type and
relation namespaces. Anonymous paths and unreflected payloads make a snapshot
incomplete and cannot produce a passing comparison. Existing builtin entities
are captured before execution and excluded by exact handle, so an empty script
has an empty snapshot. Before/after snapshots of existing entities detect
mutations to preexisting builtins; those make the snapshot incomplete until
their host normalization is implemented. They cannot silently pass parity.

Diagnostics currently retain upstream error text and line/column. A structured
symbol/category extractor, normalized host metadata, event traces, retained
handle assertions, include loaders and mutation actions remain later work.
The comparator currently compares the full diagnostic text as well, a stricter
gate than line/column alone. This harness is not yet the full manifest runner.

The generated manifest retains all registrations (including upstream duplicates),
all parameter combinations, source locations and blob hashes. Update statuses
and evidence there, then regenerate Markdown. Non-TODO statuses without evidence
are rejected. Do not remove unsupported tests or translate their source into a
different language.
