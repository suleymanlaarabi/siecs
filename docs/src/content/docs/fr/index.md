---
title: Documentation SIECS
description: Découvrir SIECS avec un démarrage rapide concis, les manuels de concepts et la référence API publique C17/C++20.
---

SIECS est un ECS à archétypes pour C17 et C++20. Il combine un runtime C
compact, un wrapper C++ typé, un stockage contigu des composants, des requêtes
en cache, des systèmes, des relations, la réflexion et des outils optionnels.

## Démarrer

- [Démarrage rapide](./getting-started/) — découvrir le modèle dans un parcours
  concis.
- [Compiler et intégrer](./building/) — choisir la distribution autonome ou
  Bake.
- [Cookbook](./cookbook/) — patterns courts pour les tâches ECS courantes.

## Manuels

Si vous découvrez les ECS, lisez les pages dans cet ordre :

1. La [théorie ECS](./theory/) explique les entités, composants, tables et
   requêtes.
2. Les [entités](./entities/) et [composants](./components/) expliquent les
   données et leur durée de vie.
3. Les [requêtes](./queries/) expliquent la correspondance et l’itération.
4. Les [systèmes](./systems/) expliquent l’ordonnancement et la logique par
   frame.

Continuez ensuite avec les manuels avancés :

- [Stockage par archétypes](./archetype-ecs/)
- [Ressources](./resources/)
- [Observateurs](./observers/)
- [Relations](./relations/)
- [Héritage](./inheritance/)
- [Modules](./modules/)
- [Concevoir avec SIECS](./ecs-design/)
- [Explorateur REST](./rest/)

## Référence API

- [API C et C++](./reference/api/)
- [Stabilité de l’API](./reference/stability/)

Tous les exemples utilisent la façade publique `<siecs.h>`. Le runtime C
utilise C17 et le wrapper typé utilise C++20.
