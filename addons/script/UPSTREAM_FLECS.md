# Flecs Script provenance

Repository: https://github.com/SanderMertens/flecs

Pinned commit: `fa788c60c87b4effc1124027db86ef0f30a778d4`.

SiECS baseline: `c41a4a220e2d6fabfa1c3cf7c0461647c5c765fd` (verified locally).

The exact source, public headers, generic parser prerequisites, C++ mixins,
documentation and test blobs are recorded in
`test/compat/flecs_manifest.json`. The generated file-by-file integration audit
is in `docs/FLECS_SCRIPT_PORT_MAP.md`.

No Script runtime file has been ported yet. Proposed local paths in the port
map are destinations, not claims that those files exist. No production Bake
package is declared until the native backend and frontend exist.

| Local file | Upstream file | Upstream commit | Upstream blob | Import date | Major deviations and reason |
| --- | --- | --- | --- | --- | --- |
| `addons/script/LICENSE.flecs` | `LICENSE` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `84c701ef1436dee2c8a9344693fe877eee2a2b3c` | 2026-11-02 | None; verbatim license. |

For every future substantive source port, add a row with its actual local
path, immutable upstream blob, date and each integration deviation. Preserve
the upstream copyright notices. Do not mark a planned port as imported.

The browser documentation at https://www.flecs.dev/flecs/FlecsScript.html can
move independently. `docs/FlecsScript.md` at the pinned commit is the frozen
documentation source; its blob is in the manifest.
