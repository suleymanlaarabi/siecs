# Third-party notices

The compatibility project uses Flecs as an external test oracle and as the
source for the planned Script frontend port. Flecs is not linked into SiECS.

- Project: [Flecs](https://github.com/SanderMertens/flecs)
- Revision: `fa788c60c87b4effc1124027db86ef0f30a778d4`
- License: MIT, reproduced verbatim in [LICENSE.flecs](LICENSE.flecs).
- Copyright (c) 2025 Sander Mertens.
- Portions Copyright (c) Meta Platforms, Inc. and affiliates.

The oracle links the separately built upstream library. Distributing that
executable also requires distributing its MIT notice. No upstream runtime
source has been copied into the production addon at this stage.

The generated compatibility manifest contains upstream testcase names, source
paths, declarations, identifiers and hashes. Its provenance is the revision
above; regeneration reads immutable git blobs, never the moving branch.
