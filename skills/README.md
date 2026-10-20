# SIECS agent skill

The `siecs` skill teaches the current core C17 and C++20 APIs and ECS design.
Its short entry point routes an agent to the relevant language and subject;
the API catalogues are reference material, not required startup reading.
The package includes shared filter tables, callback binding rules and usage
cases for C and C++; normal core usage does not require reading SIECS sources.

## Install for Codex

From your application's directory, with Node.js 22.20 or later:

```sh
npx skills add suleymanlaarabi/siecs --skill siecs -a codex
```

Add `-g` to install across projects. Then invoke `$siecs`, or describe a SIECS
coding task and let Codex select it. Start a new session if it does not appear.

```sh
npx skills check
npx skills update siecs
npx skills remove siecs -a codex
```

To select other supported agents, omit `-a codex` and use the installer's
selection. The same package follows the open Agent Skills format; only Codex
has dedicated presentation metadata.

## Install without Node

Copy the **entire** `skills/siecs` directory from this repository to either:

- `.agents/skills/siecs` inside your application, for that project;
- `~/.agents/skills/siecs`, for all your Codex projects.

Keep one installed copy per scope. To update a manual installation, replace the
whole `siecs` directory; to uninstall, remove it. The installed skill needs no
checkout of SIECS, Python, generator, or network access to read its references.
Your application still needs the SIECS headers and runtime to compile.

## Maintain the content

When the public API changes, update the corresponding language guide and API
catalogue in the same change. Keep syntax, lifetime rules, and examples aligned
with the current headers. Describe new functionality in the relevant topic;
add a root routing row only for a new family of tasks.

Describe inherited builder methods as well as directly declared methods. Include
syntax, useful combinations, constraints and lifetime behavior; a signature
catalogue alone is not a usage guide. Keep shared filters/parameters in their
focused references and link them from every applicable language guide.

Keep generic C/C++ advice out. Put unusual SIECS contracts near the affected
operation. Preserve short descriptions and selective reading rather than
adding mandatory preparatory documents or requiring the agent to inspect runtime
sources. Keep the skill revision aligned with the consuming application's API.

Sources for the packaging conventions: [Agent Skills](https://agentskills.io/specification),
[Codex skills](https://developers.openai.com/codex/skills/), and the
[Skills installer](https://github.com/vercel-labs/skills).
