# Flecs Script provenance

Repository: https://github.com/SanderMertens/flecs

Pinned commit: `fa788c60c87b4effc1124027db86ef0f30a778d4`.

SiECS baseline: `c41a4a220e2d6fabfa1c3cf7c0461647c5c765fd` (verified locally).

The exact source, public headers, generic parser prerequisites, C++ mixins,
documentation and test blobs are recorded in
`test/compat/flecs_manifest.json`. The generated file-by-file integration audit
is in `docs/FLECS_SCRIPT_PORT_MAP.md`.

The addon now ports the parser, tokenizer, AST construction, interpolation
fragmentation and typed operator kernels. Evaluation uses the native SiECS
backend. Managed scripts, IR, templates and async evaluation remain unfinished.

| Local file | Upstream file | Upstream commit | Upstream blob | Import date | Major deviations and reason |
| --- | --- | --- | --- | --- | --- |
| `addons/script/LICENSE.flecs` | `LICENSE` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `84c701ef1436dee2c8a9344693fe877eee2a2b3c` | 2026-11-02 | None; verbatim license. |

| `addons/script/src/parser.c` | `src/addons/script/parser.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `6f49aba9e21a577852d5b5c78e8410410934c34b` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/src/ast.c` | `src/addons/script/ast.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `415a3c4a525581817ae2a7aff3645865be4d71b0` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/src/ast.h` | `src/addons/script/ast.h` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `c72f711be5546e592e888a8cce69d1fdd1cfc1bf` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/src/expr/ast.c` | `src/addons/script/expr/ast.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `6163ad8aec2a120f26481784783dbae48e35319b` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. Constructors only; lexical kinds defer binding to sireflect. |
| `addons/script/src/expr/ast.h` | `src/addons/script/expr/ast.h` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `d54098a6774b75177ae46c9a039af3bcf38bd9ce` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/src/expr/parser.c` | `src/addons/script/expr/parser.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `212b04402815a6782c823ff34d62d27d0f95d52b` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. Parsing only; Flecs world-based expression API excluded. |
| `addons/script/src/expr/util.c` | `src/addons/script/expr/util.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `ceb256c10f31a2144790b2a36eb0678dce3685ae` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. Independent string helpers only. |
| `addons/script/src/expr/operators.c` | `src/addons/script/expr/util.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `ceb256c10f31a2144790b2a36eb0678dce3685ae` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. Typed kernels only; native dispatch and overflow validation. |
| `addons/script/src/expr/interpolate.c` | `src/addons/script/expr/visit_type.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `f513ea6fb70be79830c6e6357556a99bdcd956d2` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. Lexical fragmentation only; native evaluation; format specifiers rejected. |
| `addons/script/src/parser/parser.h` | `src/addons/parser/parser.h` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `e9f6e8e24dc94c82358e500bde2e7aa90a38b112` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/src/parser/grammar.h` | `src/addons/parser/grammar.h` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `e6f61160635e78ee2f65086b3127984fd8164fe9` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/src/parser/tokenizer.c` | `src/addons/parser/tokenizer.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `3c1edd1f6a3532754b7fb53dd214e12590a36cf9` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/src/parser/tokenizer.h` | `src/addons/parser/tokenizer.h` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `f50c426d9bca85ebb798871d7eb9b571e8a1d226` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/src/parser/strutils.c` | `src/addons/parser/strutils.c` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `d04a883229b50e7c89d6e6ef713a20865f21bdeb` | 2026-11-02 | Native platform/arena adapter; siecs_ internals; no explicit world arguments. |
| `addons/script/include/siecs_script.h` | `include/flecs/addons/script.h` | `fa788c60c87b4effc1124027db86ef0f30a778d4` | `9b1a321bf5dbfb8d57f1cff64bb7568d27669eb2` | 2026-11-02 | Implemented API subset; native active world; no world parameters. |

For every future substantive source port, add a row with its actual local
path, immutable upstream blob, date and each integration deviation. Preserve
the upstream copyright notices. Do not mark a planned port as imported.

The browser documentation at https://www.flecs.dev/flecs/FlecsScript.html can
move independently. `docs/FlecsScript.md` at the pinned commit is the frozen
documentation source; its blob is in the manifest.
