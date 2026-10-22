# RFC — `sigpu` v2 : rendu ECS-natif sur siecs

- **Statut** : proposition révisée (v2). Les prérequis core (§15, M1), `siwindow` (§11, M2), la preuve Slang/SDL (§10, M3) et les fondations de `sigpu` v2 (§9, M4.1, dans `addons/gpu_next`) sont faits ; le render graph, les draw features et `sirender3d` ne sont pas encore disponibles.
- **Portée** : `addons/window` (nouveau), `addons/gpu`, `addons/render3d` (nouveau), adaptation de `addons/ui` et `addons/spatial`, prérequis dans le core siecs.
- **Choix structurants** : SDL3 GPU appelé directement, sans couche d'abstraction ; shaders Slang avec réflexion ; une seule abstraction de rendu, la *draw feature* ; données chaudes dérivées des tables siecs grâce à la détection de changements.
- **Référence de lecture** : checkout du 21 octobre 2026 (scheduler en DAG, systèmes `parallel` en portions, entity index paginé inclus). Les différences avec la v1 de ce RFC sont résumées en annexe A.

## 1. Objectifs et non-objectifs

### 1.1 Première livraison

- Fenêtres et entrées séparées du renderer (`siwindow`).
- SDL3 GPU comme seule API graphique, appelée directement.
- Buffers, textures, samplers, programmes, géométries, matériaux génériques.
- Render, compute, copy, render-to-texture, présentation ; plusieurs vues et fenêtres sur un device.
- API *draw feature* publique ; `sirender3d` et `siui` écrits uniquement avec elle.
- Parité de `gpu_city`, `example/cpp` et `example/ui` : primitives, transformations, couleurs, éclairage existant, ombres, fog, bloom, MSAA, picking, scènes statiques.
- Aucun type SDL dans les headers publics.

### 1.2 Hors première livraison

PBR complet, lumières ponctuelles, import glTF/images, hot reload, culling GPU, bindless, aliasing mémoire, enregistrement parallèle, plusieurs devices, persistance des graphs custom. Le design ne les empêche pas : §6.6 indique où le culling GPU s'insère.

### 1.3 Invariants

- Scène immobile : zéro upload d'instances, zéro compilation de graph, zéro création de pipeline, zéro allocation générale après préchauffage.
- Caméra en mouvement : seuls les uniforms de vue et les listes d'indices visibles sont uploadés.
- Aucun ajout/retrait de composant ou relation causé par la visibilité, le LOD ou l'ordre des draws.
- Coût par frame proportionnel aux tables **modifiées** pour l'extraction, aux cellules testées pour le culling, aux lots pour l'enregistrement.
- Une ressource manquante ne provoque ni accès invalide ni présentation de contenu indéfini.

Ces invariants sont vérifiés par les compteurs de `GpuStats` (§18) dans les tests de `sigpu` (device SDL réel sur le driver vidéo `offscreen`), pas par des comparaisons avant/après.

## 2. Décisions

1. **Les tables siecs sont l'unité de travail.** Une table est extraite, uploadée et partitionnée comme un bloc. Une table inchangée coûte une comparaison de tick (§15.1).
2. **Une seule abstraction de rendu** : la draw feature (§6). Primitives 3D, meshes, rectangles et texte UI, rendus custom l'utilisent. Les passes non géométriques (compute, fullscreen) utilisent les callbacks de pass (§8.4).
3. **Relations seulement là où siecs apporte quelque chose** : regroupement en tables (`DrawGeometry`), reverse index consulté souvent (`DrawMaterial`, `RendersTo`), nettoyage (`ChildOf`). Les références de configuration sont des champs `ecs_entity_t` réflexés, remappés par `ecs_save`.
4. **Le graph suit la même règle que le scheduler siecs** : une séquence ordonnée, dont les dépendances et barrières sont déduites des conflits d'accès (§8.2). Pas d'arêtes de dépendance à écrire à la main.
5. **Les instances vivent dans des plages GPU persistantes par table ; chaque vue n'uploade que des indices visibles.** Les données d'instance sont partagées par toutes les vues et cascades.
6. **Les paramètres de matériau sont dans un storage buffer indexé.** Deux matériaux qui ne diffèrent que par leurs uniforms ne coupent pas un lot.
7. **La disponibilité est une valeur** (`GpuResourceState`), jamais un tag qui fait migrer des entités.
8. **Une ressource soumise reste vivante jusqu'à sa fence.** Le compteur de frames CPU n'est pas une preuve de complétion.
9. **Les helpers C/C++ construisent les mêmes entités que l'API ECS directe.** Pas de registre parallèle de scène, de matériaux ou de queries.
10. **SDL3 GPU est la seule API graphique et `sigpu` l'appelle directement.** Pas de HAL ni de backend `null` (retirés après M4.1) : SDL garde vivants les objets libérés tant que des soumissions les utilisent, ce qui couvre déjà le rôle d'une couche de retraits. Les types SDL restent hors des headers publics ; les tests tournent sur un device réel.

## 3. Architecture

```text
application ── passes custom, features custom
   │
   ├── sirender3d ── sispatial          (features 3D, graph standard, picking)
   ├── siui                             (layout, features UI)
   ▼
 sigpu
   ressources ECS et états ── chemin froid (observers + files)
   draw features ──────────── chemin chaud (extraction, culling, lots)
   render graph ───────────── ordre déduit, barrières, passes utiles
   gpu_cmd_* ── SDL3 GPU (appels directs)
   │  pont privé pour les surfaces
 siwindow ── fenêtres, événements, entrées
```

`sigpu` n'importe pas `sispatial` : une feature déclare elle-même les composants qu'elle lit. Une cible offscreen ne crée pas de fenêtre ; SDL GPU exige toutefois le sous-système vidéo (§11).

## 4. Modèle de données

### 4.1 Représentation

| Donnée | Représentation |
|---|---|
| Texture, buffer, sampler, shader, programme, géométrie | Entité + descripteur réflexé (`EcsSetOnly`) + `GpuResourceState` (`EcsInheritOwned`, voir ci-dessous) |
| Références de configuration (textures d'un matériau, `size_from`, ressources d'une pass) | Champs `ecs_entity_t` réflexés |
| Matériau | Entité + **composant de paramètres de son programme** (§7) ; variantes par `IsA` + `EcsInheritShared` |
| Objet dessinable | Relations `DrawGeometry` + `DrawMaterial` + composants lus par la feature |
| Caméra | `Camera3d` + relation `RendersTo` |
| Graph et passes | Entité `GpuRenderGraph` ; passes `ChildOf` avec `GpuPass` + `GpuPassIO` |
| Feature | Entité `GpuFeature` (description inspectable) ; callback transient |
| Instances, bounds, indices visibles, lots, plan compilé | Données dérivées de `sigpu` : plages par table, arenas par vue, caches |
| Session, statistiques, files de travail | Resources de monde (`GpuSession`, `GpuStats`, …) |

`GpuResourceState` est `EcsInheritOwned` : une instance `IsA` reçoit sa propre copie par le hook `copy` du composant, qui la remet à `Pending` sans dupliquer les objets SDL. Une instance de matériau est donc une ressource à part entière. La libération des handles reste dans le `dtor` du composant.

Rangement : les assets et matériaux d'une scène vivent sous l'entité de scène (`ChildOf`). Device, fenêtres, graph généré et pipelines dérivés sont reconstruits par le code ; leur exclusion de la sauvegarde est différée (§14).

### 4.2 Relations

| Relation | Storage | Sur suppression de la cible | Rôle |
|---|---|---|---|
| `ChildOf` | ByDepth (builtin) | builtin | Possession, noms, graph → passes |
| `DrawGeometry` | ByTarget, acyclique | `EcsRemoveRelation` | Une table = une géométrie = un lot instancié candidat |
| `DrawMaterial` | Dense | `EcsRemoveRelation` | Cible par ligne ; reverse index matériau → objets pour l'invalidation |
| `RendersTo` | Dense | `EcsRemoveRelation` | Caméra → surface ou texture ; reverse index pour générer le graph |

`DrawMaterial` est Dense pour deux raisons : changer de matériau ne fait pas migrer l'entité, et le nombre de tables reste proportionnel aux géométries, pas au produit géométries × matériaux. siecs plafonne à 65 535 tables et ne libère pas les tables vides. Le coût est une lecture de cible par ligne lors de l'extraction des index de matériau, faite seulement quand la colonne de relation change (§6.3).

Aucune autre relation n'est créée par `sigpu`. En particulier : pas de relation device (un seul device par processus, porté par `GpuSession`), pas de relation surface → fenêtre (résolution cachée dans l'état transient de la surface), pas d'entités-arêtes pour les accès ou bindings.

### 4.3 Descripteurs

| Composant | Champs essentiels |
|---|---|
| `GpuDevice` | debug, frames en vol |
| `GpuSurface` | `window_slot`, mode de présentation, espace couleur |
| `GpuBuffer` | taille, usages, politique d'upload |
| `GpuTexture` | dimension, format, taille ou `size_from` + échelle, mips, layers, samples, usages |
| `GpuSampler` | filtrage, adressage, comparaison |
| `GpuShader` | chemin du pack `.gpushader`, empreinte attendue |
| `GpuProgram` | `shader`, composant de paramètres, variantes par phase graphique (entrée, raster, profondeur, blend) |
| `GpuGeometry` | layout vertex, topologie, buffers, plages, LOD, bounds locales |
| `GpuPass` | `kind`, `order`, `enabled`, programme éventuel |
| `GpuPassIO` | tableau fixe de `GpuAccess` (§8.1) |
| `GpuFeature` | composant d'instance, programme par défaut, mode d'ordre, taille de cellule |

Les contenus de buffers et textures passent par un upload à propriété explicite (copie détenue ou référence d'asset). Un helper ne conserve jamais un pointeur vers la mémoire de l'appelant. L'upload partiel précise offset, taille et pitch.

`sigpu` ne pose aucun observer sur les dessinables : la détection de changements (§15.1) les remplace. Les observers restants portent sur les descripteurs, qui sont rares et sur le chemin froid ; ils filtrent `event->component`. Un observer ciblé sur une entité ne coupe plus le chemin rapide d'`ecs_set` pour les autres événements (§15.3).

## 5. Flux de frame

| Phase | Placement | Travail |
|---|---|---|
| Input | `EcsOnLoad` | `siwindow` pompe les événements (thread principal) |
| Simulation | phases update | Jeu ; `sispatial` termine sa propagation en PostUpdate |
| `GpuSync` | après PostUpdate | Fences complétées, retraits, descripteurs modifiés, dimensionnement des plages (§6.2), créations budgétées |
| `GpuExtract` | après PreRender | Systèmes des features : tables modifiées → plages d'instances et de bounds |
| `GpuPrepare` | après Extract | Culling par vue, LOD, lots, matériaux, compilation du graph si sa révision a changé |
| `GpuRecord` | après Prepare | Acquisition de la swapchain, uploads coalescés, enregistrement |
| `GpuSubmit` | après OnRender | Soumission, fences |

Les phases sont déclarées avec `ecs_phase({ .name, .after, .before })`. Le layout `siui` est un système PreRender en pixels logiques. Après `GpuExtract`, une seule frontière de publication fournit des données stables ; aucun callback d'enregistrement ne modifie l'ECS (diagnostic en debug). Les écritures applicatives tardives s'appliquent à la frame suivante.

Les appels SDL s'exécutent sur le thread principal. L'extraction utilise les systèmes `parallel` de siecs (§6.2).

## 6. Draw features

### 6.1 Déclaration

Une feature associe une query siecs, un composant d'instance et une manière de dessiner. `sigpu` en dérive les systèmes, les plages persistantes et les lots.

```c
ECS_COMPONENT_DECLARE(CubeInstance, {
    float position[3];
    float scale[3];
    int16_t rotation[4];
});

static void ExtractCubes(ecs_iter_t *it) {
    const GlobalPosition3d *p = ecs_field(it, 0);
    const GlobalOrientation3d *q = ecs_field(it, 1);
    const GlobalScale3d *s = ecs_field(it, 2);
    CubeInstance *out = sigpu_instances(it);   /* plage de ce batch ou de cette portion */
    GpuSphere *bounds = sigpu_bounds(it);      /* NULL si pas de culling */
    for (uint32_t i = 0; i < it->count; i++) {
        out[i] = pack_cube(&p[i], &q[i], &s[i]);
        bounds[i] = cube_bounds(&p[i], &s[i]);
    }
}

ecs_entity_t cubes = sigpu_feature(&(GpuFeatureDesc){
    .name = "render3d::cubes",
    .instance = ecs_id(CubeInstance),
    .query = {
        .components = {
            ecs_in_changed(GlobalPosition3d), ecs_in_changed(GlobalOrientation3d),
            ecs_in_changed(GlobalScale3d), ecs_filter(Cuboid),
        },
        .relations = { ecs_rel(DrawGeometry), ecs_rel(DrawMaterial) },
    },
    .extract = ExtractCubes,
    .culling = GpuCullSphere,
    .ordering = GpuOrderBatched,
});
```

```cpp
gpu::feature<CubeInstance>("render3d::cubes")
    .with<Cuboid>()
    .geometry<DrawGeometry>()
    .material<DrawMaterial>()
    .culling(gpu::cull_sphere)
    .extract([](ecs::changed<const GlobalPosition3d> p,
                ecs::changed<const GlobalOrientation3d> q,
                ecs::changed<const GlobalScale3d> s,
                CubeInstance& out, GpuSphere& bounds) {
        out = pack_cube(*p, *q, *s);
        bounds = cube_bounds(*p, *s);
    });
```

`sigpu_instances(it)` retrouve la feature via `it->user_data`, la plage via `ecs_iter_table_id(it)` (§15.2) et l'offset via `it->row_offset`, la ligne de `entities[0]` dans la table. Le wrapper C++ s'appuie sur les SystemParam : `ecs::changed<const T>` (§15.1) porte le filtre, et `CubeInstance&` et `GpuSphere&` sont des paramètres de `sigpu` initialisés par portion, construits sur `ecs::table_info` (§15.2) : `id()` résout la plage, `row()` donne l'élément. Un champ partagé (hérité d'un prefab) est géré par la façade C++ ; en C, l'extraction teste `ecs_field_is_shared` comme aujourd'hui.

Une feature sans `DrawMaterial` (UI, debug) fournit un programme par défaut et un composant de paramètres optionnel par vue.

### 6.2 Systèmes générés

`sigpu_feature` crée un système dans `GpuExtract` :

- les termes lus sont des termes `ecs_in_changed` (§15.1) : seules les tables dont une colonne lue ou la structure a changé depuis la dernière exécution du système sont visitées ; une table ignorée l'est avant tout marquage d'écriture ;
- `parallel = true` avec un `chunk_size` configurable. Le stockage de la feature est atteint par `user_data`, ce que les systèmes `parallel` autorisent déjà. Chaque portion écrit les lignes `[it->row_offset, + count)` de la plage de sa table : les écritures sont disjointes par construction, et seul ce système touche ce stockage pendant `GpuExtract`. La barrière de phase ordonne les lecteurs de `GpuPrepare` ;
- le dimensionnement des plages se fait avant, dans un système série de `GpuSync` qui itère la query de la feature : elle liste déjà les tables non vides avec leur `count`, sans API dédiée.

`ecs_iter_structure_changed(it)` (ajout, retrait, swap-remove, migration) force la réextraction de la table entière. Sinon seules les portions visitées sont marquées sales et coalescées en uploads.

Les écritures faites via un pointeur obtenu par `ecs_get` ne sont pas détectées : l'application appelle `ecs_modified`, comme le contrat du core le demande déjà pour `OnSet`.

### 6.3 Plages persistantes

Pour chaque couple (feature, table), `sigpu` détient une plage dans le buffer d'instances de la feature. Elle est indexée par l'identifiant de table, jamais par la position de la table dans la query : une query ordonnée insère des tables au milieu. La plage contient :

- les instances (`CubeInstance`), en storage buffer GPU, plus une copie de staging tant que l'upload n'est pas fait ;
- les bounds monde (`GpuSphere` ou `GpuAabb`), CPU seulement, utilisées par le culling et le picking ;
- l'index de matériau par instance (`uint32_t`), écrit par `sigpu` depuis `ecs_targets(it, DrawMaterial)` quand la colonne de relation change ; l'application ne le manipule pas.

Les plages sont sous-allouées dans un buffer par feature, avec croissance bornée (§9.2). Une table vide depuis N frames libère sa plage. Les indices d'instance sont `base_de_plage + ligne`.

### 6.4 Culling et partition spatiale

Chaque vue (caméra, cascade d'ombre) produit, dans son arena de frame, une liste d'indices visibles par lot. Le culling lit seulement les bounds des plages, jamais l'ECS.

Pour les plages stables (tick inchangé depuis N frames) au-delà d'un seuil de taille, `sigpu` construit une **partition en cellules** : indices triés par cellule (ordre de Morton), avec bounds et intervalle par cellule. Le culling d'une plage partitionnée teste ses cellules puis copie les intervalles visibles (`memcpy` d'indices). La partition est reconstruite seulement quand la table change.

Ce mécanisme remplace `Static3dReady`, la relation `RenderChunk`, les observers associés, le scan linéaire `find_or_create_chunk` et les buffers par chunk. Il ne fragmente pas les tables et s'applique à tout objet immobile, qu'il porte `Static` ou non.

### 6.5 LOD, lots, ordre

- Le LOD est choisi par instance visible (taille projetée, seuils et hystérésis par vue). Il fait partie de la clé de lot ; il ne modifie jamais `DrawGeometry`.
- Clé de lot sur 64 bits : pipeline, géométrie + LOD, jeu de textures du matériau, état dynamique incompatible. Tri radix par vue. Les uniforms de matériau ne font pas partie de la clé (ils sont lus via l'index de matériau).
- Fast path : si toutes les lignes d'une plage partagent le même matériau (cas courant), le tri est remplacé par un regroupement par plage.
- `GpuOrderBatched` : opaque et alpha-test, regroupement maximal.
- `GpuOrderSorted` : transparent, tri arrière-vers-avant stable par vue ; seuls les éléments adjacents compatibles fusionnent.
- `GpuOrderLayered` : UI, ordre de couche explicite puis ordre d'insertion stable ; fusion des seuls éléments adjacents compatibles, scissor inclus.

### 6.6 Enregistrement

Un lot = un draw instancié. Le vertex shader reçoit l'indice visible comme attribut par instance, lit l'instance dans le storage buffer de la feature et l'index de matériau, puis le bloc de matériau dans le storage buffer du programme.

Le culling GPU s'insère plus tard sans changer l'API : un compute écrit les mêmes listes d'indices et les arguments indirects.

## 7. Matériaux et programmes

### 7.1 Le composant de paramètres désigne le programme

Le pack Slang décrit le bloc de matériau du programme : uniforms, textures et samplers. `sigpu` lui associe un composant :

- **statique** : `ECS_COMPONENT_DECLARE(StandardMaterial, { uint8_t base_color[4]; ecs_entity_t albedo; ecs_entity_t albedo_sampler; })`, validé contre la réflexion par `sigpu_program_params(program, ecs_id(StandardMaterial))` ;
- **dynamique** : généré depuis la réflexion avec `ecs_component_dynamic_init`, les textures et samplers devenant des champs `ecs_entity_t` (§15.5).

Un matériau est une entité portant ce composant. Le programme se déduit du type (table composant → programme interne à `sigpu`). Porter deux composants de paramètres est une erreur. Il n'y a ni relation matériau → programme ni entité de binding.

```c
ecs_entity_t brick = ecs_new_named(scene, "materials::brick");
ecs_set(brick, StandardMaterial, {
    .base_color = { 200, 90, 60, 255 }, .albedo = brick_tex, .albedo_sampler = linear,
});

ecs_entity_t red = ecs_new_named(scene, "materials::brick_red");
ecs_is_a(red, brick);
ecs_set(red, StandardMaterial, {
    .base_color = { 220, 40, 40, 255 }, .albedo = brick_tex, .albedo_sampler = linear,
});
```

L'override porte sur le bloc entier, conformément au comportement actuel de `EcsInheritShared`.

### 7.2 Préparation

À l'`OnSet` du composant (chemin froid), le matériau est mis en file. La préparation :

1. packe les uniforms dans le storage buffer du programme selon le plan de packing CPU → GPU (§10.3) ;
2. résout les textures et samplers ; une référence morte ou non prête rend le matériau `Waiting`, ses lots sont ignorés et comptés ;
3. attribue un slot de matériau stable et un identifiant de jeu de textures (les matériaux qui partagent les mêmes textures partagent le jeu, donc les lots).

`sigpu` maintient un index dérivé texture → matériaux pour l'invalidation lors d'une réallocation de texture. Le reverse index de `DrawMaterial` donne les objets concernés quand un matériau change de programme ou de jeu de textures.

### 7.3 Phases graphiques et pipelines

Une phase graphique (`opaque`, `shadow`, `transparent`) est une entité d'identité, distincte d'un `ecs_phase_t`. `GpuProgram` liste ses variantes par phase. La clé de pipeline contient : device, révisions des shaders, interface, layout d'instance, topologie, raster/cull/front-face, depth/stencil, blend, formats et sample count. Ni taille de texture ni pointeur de swapchain.

Les pipelines sont des caches internes à `sigpu` (pas des entités), dédupliqués, préchauffés, avec un budget de créations par frame et une capacité de cache. Aucun appel de création pendant l'enregistrement.

## 8. Render graph

### 8.1 Structure

Un `GpuRenderGraph` par sortie possède ses passes (`ChildOf`). Une pass porte :

```c
GpuPass   { kind, order, enabled, program }
GpuPassIO { count, items[GPU_PASS_MAX_ACCESSES] }   /* 16, comme ecs_system_desc_t.after */
GpuAccess { resource, usage, load, store, mip/layer ou offset/taille, resolve, binding }
```

Usages : color, depth/stencil, sampled, storage read/write/read-write, vertex, index, indirect, copy source/destination. Une cible MSAA désigne sa cible de resolve dans le même accès.

Les passes géométriques référencent une ou plusieurs features et une phase graphique. Les textures de matériaux sont des lectures implicites : elles n'imposent une barrière que si le graph écrit cette texture. `sigpu` marque donc les textures écrites par un graph (`render target`) et n'ajoute les accès de matériaux qu'à ces textures-là.

### 8.2 Ordre déduit

L'ordre du graph est la séquence triée par `(order, seq)` où `seq` est attribué à la création de la pass. Pour chaque couple `i < j` dont les accès se recouvrent avec au moins une écriture, une dépendance `i → j` est déduite, exactement comme `ecs_system_index_build_plan` le fait pour les systèmes. Conséquences :

- aucun conflit n'est ambigu, aucune arête n'est à écrire à la main ;
- les barrières et transitions sont déduites des accès successifs ;
- le DAG déduit sert à l'élimination des passes inutiles et, plus tard, à l'enregistrement parallèle.

Les couches se partagent des bandes d'ordre : `GPU_ORDER_SHADOW`, `GPU_ORDER_SCENE`, `GPU_ORDER_POST`, `GPU_ORDER_UI`, `GPU_ORDER_PRESENT`. Deux caméras sur une même cible sont ordonnées par `Camera3d.order` et leur politique clear/load ; un split-screen ne clear pas l'image de l'autre caméra.

### 8.3 Validation et compilation

Le compilateur vérifie cibles absentes, usages, formats, tailles, samples, resolve, capacités du device. Il refuse l'échantillonnage d'une sous-ressource écrite comme attachement dans la même pass, et la lecture d'un contenu non initialisé ou jeté. Les tailles relatives (`size_from`) sont résolues avec arrondi défini et détection de cycle.

Racines : sorties présentées, exports, readbacks, passes déclarées à effet externe. Les autres passes sans consommateur sont éliminées.

La compilation (tri, liveness, tailles, barrières, plan plat) ne se refait que si la révision de topologie du graph change : ajout ou suppression de pass, `GpuPass`/`GpuPassIO` modifiés, format ou sample count changés. Un resize ne renouvelle que les allocations et étendues. Le nouveau plan est publié atomiquement à une frontière de frame ; un plan invalide suspend sa sortie avec diagnostic.

Une swapchain SDL n'est pas échantillonnable : la composition 3D + UI se fait dans une texture intermédiaire, la pass de présentation la copie vers la swapchain. Une acquisition sans texture (fenêtre minimisée) suspend le sous-graph de cette sortie ; les travaux offscreen continuent.

### 8.4 Passes custom

Une pass compute ou fullscreen porte un callback transient `GpuPassExecute { fn, user_data, user_data_dtor }`, détruit exactement une fois. `gpu_pass_ctx_t` est opaque : étendue, ressources déclarées, frame. Commandes : pipeline, uniforms, vertex/index, viewport/scissor, draw, indexed draw, dispatch, copies. Les noms sont résolus à la compilation ; l'enregistrement utilise des indices.

```c
static void record_blur(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *data) {
    gpu_cmd_dispatch_cover(ctx, gpu_pass_extent(ctx));  /* taille locale lue dans le pack */
}

ecs_entity_t blur = sigpu_compute_pass(graph, &(GpuComputePassDesc){
    .name = "blur", .order = GPU_ORDER_POST, .program = blur_program,
    .extent_from = blurred, .execute = record_blur,
});
sigpu_pass_sample(blur, "src", hdr, linear);
sigpu_pass_storage_write(blur, "dst", blurred);
sigpu_graph_export(graph, blurred);
```

`sigpu_pass_sample` et `sigpu_pass_storage_write` ajoutent un élément à `GpuPassIO` ; aucune entité n'est créée.


## 9. Ressources, états et complétion

### 9.1 États

`GpuResourceState` est présent toute la vie de la ressource :

```text
Pending → Waiting(dépendance) → Ready
    └──────────→ Failed(code, dépendance, message)
Ready + modification → Pending ; suppression → Retiring
```

Il porte révision demandée, révision appliquée et dernier diagnostic. Les observers des descripteurs (chemin froid) incrémentent la révision et enfilent l'entité une fois (déduplication entité + génération + révision). Ils ne compilent rien et n'appellent pas SDL. `Waiting` et `Failed` ne sont pas réessayés sans changement pertinent. Les files ont un budget par frame ; les ressources requises au démarrage peuvent être préchauffées.

Les révisions distinguent contenu (réupload de la plage), allocation (nouvelle allocation, bindings dépendants), interface (revalidation des layouts) et topologie de graph.

### 9.2 Objets SDL et mémoire de frame

- Les objets SDL (`SDL_GPUBuffer*`, `SDL_GPUTexture*`...) vivent dans des enregistrements internes ; ils ne sont ni publics ni sérialisés.
- Une ressource remplacée ou retirée libère ses objets tout de suite : `SDL_ReleaseGPU*` les garde vivants jusqu'à la fin des soumissions qui les utilisent.
- Chaque soumission de `sigpu` prend une fence, rangée dans un slot de frame. Avant d'enregistrer la soumission k, la soumission k − `frames_in_flight` doit être terminée. Pas de `WaitIdle` par frame.
- Perte du device : arrêt des soumissions, événement unique, pas d'attente infinie. Arrêt normal : producteurs → enregistrements → fences → retraits → fenêtres → device.

### 9.3 Implémentation (M4.1)

`sigpu` v2 se construit dans `addons/gpu_next` (package `sigpu_next`, header `sigpu_next.h`) jusqu'au remplacement de la v1 à la fin de M5 : la v1 et ses exemples restent intacts, et aucun binaire ne lie les deux.

- **Ressources.** `GpuDevice`, `GpuBuffer`, `GpuTexture`, `GpuSampler`, `GpuShader`, `GpuProgram` sont `EcsSetOnly` ; `ecs_with` ajoute `GpuResourceState` à chacun. Trois observers (`OnAdd`, `OnSet`, `OnRemove`, filtrés sur l'état) mettent l'entité en file une fois grâce au drapeau `queued` ; ils n'appellent jamais SDL. Les objets SDL d'une ressource vivent dans un enregistrement interne désigné par `GpuResourceState.record`. Une ressource détient des objets SDL si et seulement si elle est `Ready`.
- **`GpuSync`** (système `main_thread_only`, `no_defer`) : libérations, ouverture/fermeture du device, puis la file dans la limite du budget (`sigpu_props_t.budget`, 64 par défaut). Les dépendances (`size_from`, programme → shader, second device → device ouvert) sont des arêtes internes ; une ressource n'est retraitée que si son descripteur ou une dépendance change. Une texture dont la source est réallouée à la même taille garde son allocation.
- **Constat core.** L'instanciation `IsA` passe par le hook `copy` (pas `copy_ctor`). Une colonne n'est déplacée par `memcpy` que si le composant n'a ni `move_ctor` ni `copy_ctor` ; sinon une migration de table fait `copy_ctor` puis `dtor`, contrairement à ce que dit le commentaire de `ecs_type_ops_t`. `GpuResourceState` ne définit donc que `dtor` et `copy` : une migration garde l'état et son enregistrement. Aligner le core (`memcpy` dès que `move_ctor` est absent) éviterait le piège aux prochains composants qui portent des objets SDL.
- **Constat core.** Les queries excluent `Abstract` par défaut : une base `IsA` est abstraite. Les queries internes qui doivent voir toutes les ressources mentionnent `Abstract` et `Disabled` en termes optionnels.
- **Retiring** est l'état final d'une ressource dont le descripteur a été retiré.
- **Device.** Changer `GpuDevice` le rouvre ; le retirer libère tout et remet les ressources à `Pending`. Un second device attend (`Waiting`) la fermeture du premier. SDL n'expose pas la perte du device : un échec d'acquisition de command buffer, de soumission ou d'attente de fence est traité comme une perte (état `Failed(GpuErrorDevice)`, événement `GpuDeviceLost` émis une fois, plus aucune soumission, aucune attente).
- **Uploads.** `sigpu_buffer_upload` et `sigpu_texture_upload` copient les données ; elles sont appliquées dans `GpuRecord` dès que la cible est `Ready`, sur l'allocation courante, et abandonnées si elle échoue ou disparaît. Tous les uploads prêts d'une frame partagent un transfer buffer (offsets alignés sur 16) et une soumission ; sans upload prêt, aucun command buffer n'est acquis.
- **Compteurs.** `GpuStats.objects` compte les objets SDL vivants ; `created`, `released`, `uploads`, `submissions`, `frame_waits` comptent la frame. `GpuSession` n'est réécrit qu'à l'ouverture, la fermeture ou la perte du device, et à chaque soumission pour `submissions`.
- **Devices.** Les observers marquent les devices à revoir ; `GpuSync` ferme le device ouvert si son descripteur a changé ou disparu (et le rouvre en premier s'il a changé), puis ouvre le premier autre device si aucun n'est ouvert, les autres attendant (`Waiting`). Le device passe par le même chemin d'état que les autres ressources.
- **Tests** (`test/gpu`, 32 cas) : device SDL réel sur le driver vidéo `offscreen` ; les contenus sont relus par download. La perte du device et un GPU lent ne sont pas simulables sans couche intermédiaire et ne sont pas testés.
- Pas encore faits dans ce lot : composant de paramètres et variantes de `GpuProgram` (M4.3), politique d'upload de `GpuBuffer`, révisions d'interface, préchauffage explicite.

## 10. Shaders Slang

### 10.1 Packs

Un outil de build compile les shaders avec l'API Slang et écrit des packs `.gpushader` déterministes : version de format, empreinte de toolchain, entrées, blobs par cible, limites requises, interface logique et layouts effectifs. Le runtime n'embarque pas le compilateur. Une machine Linux ne valide pas DXIL ni Metal.

Implémenté au M3 :

- outil `addons/gpu_next/tools/gpushader` (C++, API Slang), construit par `make gpushader` ; `make shaders` régénère les packs. Ce n'est pas un projet Bake : `bake rebuild . -r` découvre tous les projets du dépôt, et la CI n'a pas Slang. Les packs générés sont versionnés, donc seul l'auteur de shaders a besoin de Slang ;
- format (`src/pack/gpushader_format.h`) : en-tête, tables d'entrées, ressources, champs et entrées vertex, puis le SPIR-V. Uniquement des `uint32_t` et des tableaux de `char`, sans padding ni chemin ni date : deux compilations donnent les mêmes octets. `gpushader --dump` l'affiche en texte ;
- chargeur privé `src/pack/gpushader.c`, sans SDL depuis M4.1 : copie et valide le pack (bornes de toutes les tables, noms terminés) et renvoie la raison d'un rejet, reprise dans le message de `GpuResourceState`. Tronqué ou corrompu, il est rejeté sans accès invalide. `sigpu` crée `SDL_GPUShader` ou `SDL_GPUComputePipeline` avec les comptes stockés ;
- cible : SPIR-V 1.0 seulement. SDL crée une instance Vulkan 1.0 et la couche de validation refuse le SPIR-V 1.3 ; Slang avertit qu'une version 1.0 est ancienne (avertissement 50011, désactivé) mais sa sortie passe `spirv-val --target-env vulkan1.0`. DXIL et MSL ne sont pas produits.

### 10.2 Bindings

Les scopes vue, matériau, instance sont des conventions `sigpu`, abaissées vers les bindings SDL par stage et catégorie. L'outil impose une convention d'auteur et rejette les layouts impossibles à abaisser. Les données d'instance et de matériau sont des storage buffers lisibles en vertex et fragment.

Convention retenue au M3 :

- les ressources sont des globales sans `[[vk::binding]]` ; le scope est l'attribut `[Scope("view" | "material" | "instance" | "pass")]` sur la globale, stocké dans le pack ;
- uniforms dans un `ConstantBuffer<T>` : les uniforms globaux libres et les paramètres uniformes d'entrée sont refusés ;
- textures en samplers combinés (`Sampler2D`) : `SamplerState` séparé et texture échantillonnée seule sont refusés, SDL ne les connaît pas ;
- vertex et fragment ne font que lire les storage buffers et textures ; une ressource en écriture n'est acceptée qu'en compute.

Chaque entrée est compilée seule. La réflexion (`IMetadata::isParameterLocationUsed`) donne les globales qu'elle utilise ; elles sont rangées par catégorie, dans l'ordre de déclaration, puis les décorations `DescriptorSet`/`Binding` du SPIR-V sont réécrites vers les sets imposés par SDL (vertex 0/1, fragment 2/3, compute 0/1/2). Le pack garde, par ressource, le slot à passer à `SDL_BindGPU*`, le stride des storage buffers et la taille des blocs uniformes (padding std140 inclus : un bloc de 8 octets en occupe 16).

### 10.3 Plan de packing

Le layout CPU (sireflect) et le layout GPU (Slang) ne sont pas supposés identiques. Un plan de packing est construit une fois par couple schema CPU / interface GPU / cible : copies, offsets, strides, conversion des booléens, vecteurs, matrices, tableaux fixes ; padding initialisé. La copie directe n'est utilisée que si tout correspond exactement. Types non supportés refusés avec le chemin du champ.

Le même plan sert aux instances de feature : le composant d'instance est validé contre l'interface d'instance du programme.

Le pack fournit déjà ce qu'il faut au plan : nom, offset, taille, scalaire et dimensions de chaque champ de premier niveau des blocs. Le programme de preuve compare ces valeurs à `offsetof` et `sizeof` de ses structs C. Les champs imbriqués (struct dans struct, tableaux) restent à décrire au M4.

## 11. Fenêtres et surfaces : `siwindow`

Implémenté au M2 (`addons/window`, tests `test/window` sur le driver SDL `offscreen`).

Constat du M3 : `SDL_CreateGPUDevice` échoue tant que le sous-système vidéo n'est pas initialisé (« Video subsystem not initialized »), même sans fenêtre. Sans vidéo initialisée, `sigpu` la démarre avec le driver `offscreen`, sans fenêtre ni écran. La phrase de §3 sur la vidéo SDL se lit donc « ne crée pas de fenêtre ».

- `Window { slot, title, width, height, mode, text_input }` est écrit par l'application ; la fenêtre native est créée à l'`OnSet`. `slot` est une clé stable (`"main"`). Les champs sont des tableaux de `char`, donc sauvegardables (§15.5). `WindowState` porte l'état réel : tailles logique et en pixels, échelle, focus, minimisation.
- `KeyboardState` (tous les scancodes, en bitsets `held`, `went_down`, `went_up`) et `PointerState` (position, deltas, boutons, file de fronts bornée à 64) sont des **resources qui décrivent la fenêtre qui a le focus**, avec un champ `window`. Un système de caméra lit donc `ecs::res<const KeyboardState>` dans sa propre query. Le multi-fenêtre ajoutera des composants par fenêtre ; la resource restera « fenêtre qui a le focus ». La perte de focus relâche touches et boutons (front `Cancel`).
- Événements ciblés sur l'entité fenêtre : resize, close-requested, focus, texte (`siwindow_event_t`). Le pointeur reste dans `PointerState` ; `sigpu` en dérive ses événements de picking.
- La fermeture est une demande : l'événement est toujours émis, puis `ecs_quit` sauf avec `keep_open_on_close`. Échap émet la même demande sauf avec `keep_open_on_escape`.
- Le système `PumpWindowEvents` tourne en `EcsOnLoad` sur le thread principal.
- Pont renderer : `siwindow_find(slot)` et `siwindow_native(window)` (`siwindow/native.h`, `void*` sans type SDL). Le `sigpu` actuel exige la fenêtre `"main"` à son import ; `GpuSurface.window_slot` et l'état `Waiting` arrivent avec le render graph (M4.2).

## 12. `sirender3d`

### 12.1 Vues

- `Camera3d { fov (degrés), near, far, projection, viewport, order }` + `RendersTo`. Pas de « dernière caméra trouvée ».
- Fog, sky, ambient, bloom, shadows, MSAA : composants sur la caméra, valeurs par défaut du renderer si absents. Les resources globales actuelles (`Sky`, `Sun`, `Fog`, …) disparaissent.
- `sirender3d` génère le graph standard de chaque surface depuis le reverse index de `RendersTo` : ombres, forward, bloom, composite, puis contributions UI et présentation. Le graph n'est régénéré que lors d'un changement structurel (caméra ajoutée, option d'ombres activée, format).

### 12.2 Features

Une feature par primitive (cuboïde, sphère, cylindre) et une feature mesh générique, toutes publiques. Les primitives intégrées sont des géométries internées par device et type ; leurs LOD sont des plages de la géométrie.

### 12.3 Ombres

Chaque cascade est une vue. Les cascades réutilisent les plages d'instances et la partition en cellules. Le cache d'une cascade reste valide tant que les ticks des plages qu'elle contient, les paramètres de lumière et sa matrice ne changent pas ; ce critère remplace le compteur global `static_generation`.

### 12.4 Picking

Le picking réutilise les bounds et les cellules des plages (§6.3, §6.4), puis teste exactement les candidats. Il lit la fenêtre et la caméra désignées et convertit les coordonnées selon DPI et viewport. Les événements portent l'entité touchée et le hit ; aucun second moteur spatial.

### 12.5 Adaptation de `sispatial`

La propagation garde `ecs_order_by_depth(ChildOf)` (existant) et ajoute des termes `ecs_in_changed` sur les composants locaux. Un niveau de profondeur est recalculé pour les tables dont les composants locaux ont changé, ou entièrement si un niveau parent a changé (invalidation conservative, affinable). Les tables ignorées ne marquent pas leurs composants globaux : une hiérarchie immobile ne produit aucun changement visible par le renderer.

Les booléens de dirty globaux, `Static3dReady` et la propagation statique séparée disparaissent ; `Static` reste une indication sémantique. Les observers restants filtrent `event->component`. La lecture du parent garde `ecs_try_get`. La propagation ordonnée par profondeur reste série : les systèmes `parallel` refusent `order_by`.

## 13. `siui`

- Layout dans un système PreRender, en pixels logiques, avec les dirty flags existants.
- Rendu par deux features `GpuOrderLayered` (rectangles, texte) et une pass dans la bande `GPU_ORDER_UI`, composée dans la texture intermédiaire avant présentation.
- Plus aucun pipeline, shader ou type SDL dans `siui` ; l'atlas de texte est une `GpuTexture`.
- Encodage couleur défini par la surface (corrige le double encodage sRGB probable sur une swapchain linéaire).

## 14. Sauvegarde

| Persisté | Transient (reconstruit) |
|---|---|
| Descripteurs de textures/buffers par référence d'asset stable | Allocations, handles, `GpuResourceState` |
| Composants de paramètres des matériaux, `IsA` | Slots de matériaux, storage buffers |
| Caméras, `RendersTo`, `DrawGeometry`, `DrawMaterial` | Plages d'instances, partitions, lots |
| `GpuSurface.window_slot` | Fenêtres natives, device |
| Interfaces par nom et empreinte | IDs de composants dynamiques, pipelines |
| — | Graph généré, passes custom, features (code) |

Aujourd'hui un composant avec type ops mais sans réflexion fait échouer toute la sauvegarde. Sauvegarder un monde dont le renderer est actif est donc **hors première livraison**. Quand ce sera nécessaire, l'option à évaluer d'abord est de réflexer `GpuResourceState` avec un handle invalidé par l'identifiant de session, avant d'ajouter une politique de scène au core. Le graph v1 est construit par le code. Les données initiales sans asset vont dans un composant de blob avec codec explicite, sinon la ressource est déclarée non sauvegardable.

Chargement : importer les modules → enregistrer interfaces et features → `ecs_load` → réconcilier les descriptions → résoudre fenêtres et device → synchroniser.

## 15. Prérequis siecs

Les ajouts core nécessaires au rendu sont implémentés (commit `4124e2d`). Chaque besoin de la première version de cette section a été revérifié dans le code ; seuls ceux qu'aucun mécanisme existant ne couvre ont été ajoutés (§15.6).

### 15.1 Détection de changements (fait)

**Granularité** : par table et par composant. Une écriture marque toute la colonne de la table ; on ne sait pas quelle ligne a changé.

**Modèle.**

- Le monde porte une horloge `change_tick` (64 bits, sans gestion de débordement) qui démarre à 1.
- Chaque colonne de table porte un tick, et chaque table un `structure_tick`.
- Un nœud de système prend `t = change_tick++` à son premier démarrage. Les écritures de ses champs portent `t`, et son `last_run` vaut `t` à la complétion du nœud, jamais par batch : sinon la deuxième portion d'une table la raterait.
- « Changé » signifie `tick > last_run`. Un système ne se redéclenche donc pas sur ses propres écritures. Un système désactivé ou sauté par son intervalle accumule les changements.
- Une query ad hoc prend son tick à `ecs_query_iter` et valide `last_run` quand `ecs_iter_next` renvoie faux ; un `break` laisse les changements visibles au passage suivant. `ecs_query_count` ignore le filtre et ne valide rien.

**Écritures marquées** (un store relaxé chacune, jamais par ligne) :

| Écriture | Vue ? |
|---|---|
| `ecs_set`, `ecs_set_plain_many`, set générique, flush de commandes, `ecs_modified` | oui |
| Champ `ecs_out` / `ecs_inout` d'une query, une fois par batch ou portion | oui (toute la table, même si une seule ligne est écrite) |
| Ajout, retrait, swap-remove, migration | oui, via `ecs_iter_structure_changed` |
| Écriture via un pointeur obtenu par `ecs_get` | non, sauf `ecs_modified` après |

**API.**

```c
#define ECS_TERM_CHANGED (UINT32_C(1) << 24)
#define ecs_in_changed(c)     /* EcsIn | ECS_TERM_CHANGED */
#define ecs_inout_changed(c)  /* EcsInOut | ECS_TERM_CHANGED */
bool ecs_iter_changed(const ecs_iter_t *it, uint16_t field_index);
bool ecs_iter_structure_changed(const ecs_iter_t *it);
```

```cpp
ecs::system("ExtractCubes")
    .changed<GlobalPosition3d, GlobalScale3d>()
    .each([](const GlobalPosition3d &p, const GlobalScale3d &s) { /* ... */ });

ecs::system("ExtractCubes")
    .each([](ecs::changed<const GlobalPosition3d> p, ecs::changed<const GlobalScale3d> s,
             ecs::table_info table) {
        if (table.structure_changed() || p.has_changed()) { /* ... */ }
    });
```

En C++, `.changed<T...>()` filtre des arguments ordinaires ; le paramètre `ecs::changed<T>` porte le filtre et dit en plus, par batch, si ce champ a changé (`has_changed()`).

**Règles.**

- Plusieurs termes `changed` se combinent en OU au niveau table, en OU aussi avec `structure_tick`.
- Une table ignorée l'est avant tout marquage d'écriture. `ecs_in_changed(Local)` + `ecs_out(Global)` ne marque donc pas `Global` sur une table immobile, et « rien n'a changé » se propage le long d'une chaîne de systèmes.
- Le worker pool applique le même test et ne réclame aucune portion pour une table inchangée.
- Un champ partagé (`IsA`) compare les ticks de la colonne dans la table de la base.
- Un terme `changed` doit être un terme de champ sans traversée `up` (assert à la compilation de la query).
- Un accès déclaré en écriture compte comme écriture : on lit avec `ecs_in`.

**Coût.** Un système sans terme `changed` paie un store par champ en écriture et par batch. Un système filtré paie au plus 16 comparaisons par table, et les tables inchangées ne coûtent plus rien en aval.

### 15.2 Identité de table sur l'itérateur (fait)

```c
uint16_t ecs_iter_table_id(const ecs_iter_t *it);   /* stable pour la vie du monde */
```

En C++, le paramètre système `ecs::table_info` expose `id()`, `row_offset()`, `row()` (ligne de l'entité courante) et `structure_changed()`.

`it->table_idx` est une position dans le cache de la query, décalée par les insertions d'une query ordonnée ; seul l'identifiant de table est stable. `it->row_offset` donne la ligne de `entities[0]` dans la table, y compris dans les portions d'un système `parallel`.

Il n'y a pas d'API de découverte des nouvelles tables : itérer la query de la feature dans un système série de `GpuSync` liste déjà ses tables non vides avec leur `count` (§6.2).

### 15.3 Observers ciblés par événement (fait)

`ecs_table_observed` renvoyait vrai pour toutes les tables dès qu'un seul observer ciblé sur une entité existait, quel que soit l'événement, ce qui coupait le chemin rapide d'`ecs_set` dans tout le monde. Un compteur d'observers ciblés par événement builtin le remplace. Un observer ciblé sur un événement custom (picking) ne touche plus `OnSet`.

### 15.4 Corrections (fait)

- `ecs_query_entities` lisait `it.entities[i]` un élément après la fin du batch.
- `ecs_query_each` et `ecs_query_entities` sautaient la boucle pour la query d'identifiant 0.
- Le commentaire d'`EcsOnRemove` disait « après » alors que l'événement est émis avant le retrait.

Reste connu, non bloquant : `ecs_observer_trigger` n'est pas protégé en contexte `parallel`. `sigpu` n'émet ses événements que depuis le thread principal.

### 15.5 Réflexion

Vérifié par un programme de test (aller-retour `ecs_save` / `ecs_load` sous ASan et UBSan, références remappées vers de nouvelles entités). Aucun ajout core n'est nécessaire.

| Cas | Réflexion | Sauvegarde |
|---|---|---|
| Tableau fixe de structs contenant des `ecs_entity_t` (`GpuPassIO`) | Arbre complet : `ecs_entity_t` porte le rôle `siecs.role="entity"` (`src/bootstrap.c`) | Récursion dans les structs et les tableaux ; chaque entité est écrite en index local à la scène et remappée au chargement, 0 reste 0 (`src/save.c`) |
| Tableaux de `char` (`Window.slot`, `GpuAccess.binding`) | Tableau de `char`, pas chaîne (seul `char*` est une chaîne) | Copie octet par octet, aller-retour exact |
| Champ entité d'un composant dynamique | `ecs_component_dynamic_init` résout le type `ecs_entity_t` déclaré dans `fields`, qui porte le rôle | Même encodage que pour un composant statique |

Contraintes à respecter côté `sigpu` :

- une référence doit être déclarée `ecs_entity_t`, jamais `uint64_t` : sinon elle est sauvegardée en bits bruts, sans remappage ;
- la sauvegarde échoue si une référence désigne une entité hors de la scène ;
- la table de remappage ignore la génération : un handle périmé dont l'identifiant a été recyclé est remappé vers la nouvelle entité. Comparer aussi la génération dans `ecs_scene_build_entity_map` est un durcissement possible, non bloquant ;
- l'addon REST expose les tableaux de `char` comme des tableaux de caractères, ce qui ne touche que la lisibilité du JSON.

### 15.6 Retirés ou différés

| Besoin de la v2 initiale | Décision | Raison |
|---|---|---|
| `EcsInheritNone` | Retiré | `EcsInheritOwned` copie via le hook `copy` ; un `copy` qui remet l'état à `Pending` donne à l'instance son propre état (§4.1). La libération reste dans le `dtor`. |
| Observers *scoped* | Retiré | Les dessinables n'ont plus d'observers `sigpu` ; les observers restants portent sur des descripteurs rares et filtrent `event->component`. |
| Accès partitionné en `parallel` | Retiré | Le stockage d'une feature n'est touché que par son système pendant `GpuExtract`, via `user_data`, ce qui est déjà autorisé (§6.2). |
| Terme parent via `ChildOf` | Retiré | Optimisation de `sispatial`, pas un besoin du rendu. |
| Plan incrémental du scheduler | Retiré | L'extraction n'émet pas de commandes, donc ni publication ni reconstruction du plan. |
| `FrameCount`, `ElapsedTime` | Retiré | Les slots de frame sont internes à `sigpu` ; `DeltaTime` suffit pour le temps shader. |
| Polling des tables (`ecs_world_table_count`, `ecs_query_has_table`) | Retiré | Remplacé par l'itération de la query de la feature (§15.2). |
| `ecs_iter_row`, `ecs_iter_commit` | Retiré | `it->row_offset` existe ; la validation d'une query ad hoc se fait à la fin de l'itération. |
| `SceneTransient`, politique de scène | Différé | Seulement utile pour sauvegarder avec un renderer actif, hors première livraison (§14). |

### 15.7 Plus tard

GC des tables vides et identifiants de table sur 32 bits ; relations multi-cibles et données de relation. Ces dernières permettraient d'exprimer accès et bindings en paires, sans être nécessaires ici.


## 16. Jalons

| Jalon | Contenu | Condition de sortie |
|---|---|---|
| M1 — core | §15.1 à §15.5 | Fait : tests C et C++ verts en debug, release et `tsan`, `distr` à jour, docs `queries.mdx`, `systems.mdx`, `observers.mdx` |
| M2 — `siwindow` | Fenêtre et entrées extraites de `sigpu` | Fait : `test/window` vert (debug, release, ASan) ; `example/cpp`, `gpu_city` et `example/ui` via `siwindow` |
| M3 — preuve Slang/SDL | Outil de pack, draw instancié lisant indices visibles + storage buffers, un dispatch | Fait : `example/gpu_proof` sur Vulkan (Intel Arc, Mesa), couche de validation active et muette, debug et ASan/UBSan ; DXIL et Metal non testés |
| M4.1 — fondations `sigpu` | Session et phases, ressources et `GpuResourceState`, uploads, frames en vol, sur SDL3 GPU direct (§9) | Fait : `test/gpu` (32 tests, device réel) vert en debug, release et ASan/UBSan ; `gpu_proof` sur les ressources `sigpu`, 4 verdicts ok sur Vulkan, validation muette |
| M4.2 — render graph | `GpuRenderGraph`, passes, ordre déduit, validation, élimination, barrières, surfaces et présentation, passes custom (§8, §11) | Tests « Graph » et « Fenêtres » |
| M4.3 — features | `sigpu_feature`, extraction `parallel`, plages, culling et cellules, LOD, lots, matériaux, plan de packing, `GpuStats` complet (§6, §7, §10.3) | Tests « Features », « Culling / lots », « Matériaux », invariants §1.3 |
| M5 — `sirender3d` + `siui` | Primitives, ombres, bloom, MSAA, picking, UI par features ; conversion des exemples ; suppression de l'ancien backend dans le même lot | Parité fonctionnelle des exemples (§1.1) |

M4 et M5 forment un lot de livraison : pas de double API publique ni d'adaptateur legacy. Pendant le développement, `sigpu_next` vit à côté de la v1 sans être utilisé par ses exemples ; le lot final le renomme en `sigpu` et supprime la v1. M3 est le seul jalon qui pouvait remettre en cause le design (storage buffers lisibles en vertex, `first_instance`) ; il précède tout code `sigpu` v2.

Résultats du M3 (`bake run example/gpu_proof`, rendu offscreen relu pixel par pixel ; `--window` pour le voir) :

| Point | Verdict sur Vulkan |
|---|---|
| Dispatch compute qui écrit les instances et les indices visibles, buffer d'indices utilisé ensuite comme vertex buffer | ok |
| Storage buffer lu en vertex (position et taille de l'instance) | ok |
| Storage buffer lu en fragment (bloc de matériau via l'index d'instance) | ok |
| `first_instance` décale le vertex buffer par instance (§6.6) : le second draw commence à 16 et dessine la moitié basse | ok |

Le design de §6.6 tient. L'indice visible passe par un attribut par instance, pas par `SV_InstanceID` : la doc SDL précise que `first_instance` n'est pas compatible avec les builtins d'identifiant, définis différemment selon les API, alors qu'un attribut par instance est décalé de la même façon partout.

## 17. Tests et acceptation

| Domaine | Cas indispensables |
|---|---|
| Core (fait) | Suite `change` et tests ajoutés à `query`, `observer` et à la suite C++ (§15) |
| `GpuResourceState` | Instance `IsA` neuve à `Pending`, handles non dupliqués, libération unique dans le `dtor` |
| Cycle de vie | Création hors ordre, coalescence, `Waiting` sans retry, échec puis correction, retrait avant/après préparation, handle périmé |
| Complétion | Frames en vol, resize avec soumissions actives (GPU lent et perte du device non simulables, §9.3) |
| Graph | Ordre déduit RAW/WAR/WAW, clear/load/store, lecture non initialisée rejetée, pass inutile éliminée, export conservé, resize sans recompilation |
| Features | Table inchangée non extraite, swap-remove → réextraction, plage indexée par identifiant de table, portions parallèles disjointes, plage libérée, index de matériau mis à jour sur retarget |
| Culling / lots | Tous/aucun visibles, partition reconstruite seulement sur changement, LOD sans mutation, fusion inter-matériaux (même jeu de textures), transparent ordonné, UI en couches avec scissor |
| Matériaux | Override `IsA`, texture morte → `Waiting`, packing CPU ≠ GPU, padding initialisé |
| Fenêtres | DPI, minimisation, close refusé, deux fenêtres, split-screen, offscreen sans vidéo |

Les tests de `sigpu` tournent sur un device SDL réel (driver vidéo `offscreen`) et valident les invariants par les compteurs ; ils ne vérifient pas l'image. Les captures réelles couvrent ombres, MSAA, bloom, resize, multi-vue et UI, avec tolérances explicites.

Validation core, pour tout nouveau changement : `bake run test` puis `bake run test/cpp`, debug et release, config `tsan` pour les portions parallèles, `bake rebuild test -r` après tout changement d'ABI ou de layout, puis `make distr`, `make check-distr`, `make check-distr-standalone`, `make check-distr-cpp-standalone`. Chaque addon a son propre projet de test ; le projet `test` du core ne dépend pas de SDL.

## 18. Observabilité

`GpuStats`, par vue et par graph : temps CPU par phase, tables extraites/ignorées, instances candidates/visibles, lots et draws, changements de pipeline et de bindings, octets uploadés, créations de ressources et pipelines, cache hits/misses, croissances, mémoire réservée/utilisée, passes éliminées/suspendues, lots ignorés et causes. Le temps GPU n'est exposé que si SDL le mesure réellement.

Ces compteurs servent au diagnostic et aux tests d'invariants (§1.3). Il n'y a pas de campagne de comparaison avec l'ancien renderer.

## Annexe A — Changements par rapport à la v1

| v1 | v2 | Raison |
|---|---|---|
| 15 relations, entités-arêtes pour accès, dépendances et bindings | 3 relations + `ChildOf` ; références en champs `ecs_entity_t` | Relations siecs exclusives : les arêtes multipliaient entités et règles |
| `DependsOn` obligatoire, ambiguïté = erreur | Ordre `(order, seq)` + dépendances déduites | Même règle que le scheduler siecs |
| `DrawMaterial` ByTarget, un bloc d'uniforms par matériau | Dense + storage buffer indexé | Moins de tables, pas de migration, moins de draws |
| Compaction des instances par vue | Plages persistantes par table + indices visibles | 4 octets par instance visible ; données partagées entre vues |
| Listes de changements `sispatial` avec curseurs, révisions des transforms | Détection de changements core par (table, colonne) | Couvre aussi les écritures par query |
| Chunks statiques via `Static3dReady` / `RenderChunk` | Partition en cellules dérivée par plage stable | Pas de fragmentation, pas d'observers |
| `GpuBindingSet`, `GpuBinding`, héritage de bindings par clé | Champs du composant de paramètres + `IsA` | Héritage existant de siecs |
| `OnDevice`, `PresentsTo`, `UsesProgram`, `SizedBy`, `ExecutesPhase`, `VariantFor` | Session, état transient, type du composant, `size_from`, variantes dans `GpuProgram` | Données plutôt que relations |
| Graph custom persisté, recettes nommées | Graph construit par le code, non sauvegardé | Périmètre v1 |
| Extraction séquentielle par hypothèse | Systèmes `parallel` en portions, écritures disjointes via `user_data` | Fonctionnalité du scheduler désormais disponible |
| Pas d'API publique de couche de rendu | Draw features utilisées par `sirender3d` et `siui` | Une seule abstraction prouvée par ses consommateurs |

## Annexe B — Références

Sources locales à garder alignées : `include/siecs.h`, `src/query.c`, `src/storage/query_index.c`, `src/storage/system_index.c`, `src/worker_pool.c`, `src/system.c`, `src/component_ops.c`, `src/command_buffer.c`, `src/relation.c`, `src/inheritance.c`, `src/observer.c`, `src/save.c`, `addons/spatial/src/spatial.c`, `addons/gpu/src/ecs/primitives.c`, `addons/gpu/src/ecs/static_chunks.c`, `addons/gpu/src/ecs/render_schedule.c`, `addons/ui/src/render/render.c`.

Documentation externe :

- [SDL — acquisition de swapchain](https://wiki.libsdl.org/SDL3/SDL_WaitAndAcquireGPUSwapchainTexture)
- [SDL — command buffers](https://wiki.libsdl.org/SDL3/SDL_AcquireGPUCommandBuffer)
- [SDL — soumission avec fence](https://wiki.libsdl.org/SDL3/SDL_SubmitGPUCommandBufferAndAcquireFence), [consultation de fence](https://wiki.libsdl.org/SDL3/SDL_QueryGPUFence)
- [SDL — libération de texture](https://wiki.libsdl.org/SDL3/SDL_ReleaseGPUTexture)
- [SDL — shaders graphics](https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader), [compute](https://wiki.libsdl.org/SDL3/SDL_CreateGPUComputePipeline)
- [Slang — réflexion](https://shader-slang.org/slang/user-guide/reflection.html)

Figé au M3 : Slang `v2026.18.2` (paquet `shader-slang`), SPIR-V 1.0, matrices column-major, convention d'auteur de §10.2 ; `first_instance` et storage buffers en vertex validés sur Vulkan.

Constaté au M4.1 : SDL 3.4 n'a pas de requête de perte du device (§9.3).

Restent à figer : comportement de `first_instance` et des storage buffers en vertex sur D3D12 et Metal (non testés, pas de machine) ; capacités minimales des plateformes de livraison ; budgets par défaut (frames en vol, staging, créations par frame) fixés à partir des compteurs.
