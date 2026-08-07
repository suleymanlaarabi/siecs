---
title: Politique de stabilité API
description: Garanties de compatibilité, durée de vie et règles de changement pour l’API publique SIECS.
---

Cette politique concerne les déclarations accessibles depuis `siecs.h` ou
`siecs/cpp.hpp`. Les noms sous `ecs::detail` ne sont pas des API publiques.

## Niveaux

- **Stable** : le nom, la convention d’appel, l’ownership et le comportement
  observable sont conservés pendant une version majeure.
- **Expérimental** : l’API peut changer ou disparaître dans une version mineure.
- **Déprécié** : l’API reste compatible pendant une version majeure et indique
  son remplacement.

Les handles appartiennent au monde actif. Les pointeurs d’accès sont empruntés et
peuvent être invalidés par une migration de table.

## Revue obligatoire

Un changement d’API publique doit mettre à jour son contrat de déclaration, ses
préconditions, ses règles d’ownership, un test ciblé et la référence API lorsque
le comportement visible change.
