---
title: AI Coding Agents
description: Install the SIECS skill for Codex, with focused C17 and C++20 references and ECS design guidance.
---

SIECS provides an [Agent Skills package](https://github.com/suleymanlaarabi/siecs/tree/main/skills/siecs)
for coding with its core C17 and C++20 APIs. It guides agents toward composition,
small reusable capabilities, explicit dependencies, and data-oriented design.

The entry point stays short. Guides are separated by language and subject, with
API catalogues for looking up individual signatures. Agents read only the
references relevant to the task. Addons are outside this package's scope.

## Install for Codex

From your application's directory, using Node.js 22.20 or later:

```sh
npx skills add suleymanlaarabi/siecs --skill siecs -a codex
```

Add `-g` for installation across projects. Invoke `$siecs` in Codex, or describe
a SIECS task and let the agent choose the skill. Start a new session if a newly
installed skill does not appear.

```sh
npx skills check
npx skills update siecs
npx skills remove siecs -a codex
```

Omit `-a codex` during installation to select other agents supported by the
installer. The package uses the open Agent Skills format; Codex also receives
display metadata.

## Install manually

Copy the complete `skills/siecs` folder into `.agents/skills/siecs` in your
application, or into `~/.agents/skills/siecs` for all your Codex projects.
Keep its references and catalogues together. Replace the complete folder to
update a manual installation, or remove it to uninstall.

Reading the installed skill needs no SIECS checkout, Python tooling, or network
access. Building the application still requires the SIECS headers and runtime.
Use a skill revision matching the application's SIECS API. Filter combinations,
callback parameters, lifecycle rules and API signatures are bundled with the
skill so normal core usage does not require a SIECS source checkout.
