# Feuille de route — CoriumEngine

Objectif du projet : exercice d'architecture et de contraintes d'un projet de grande envergure
(moteur de jeu). L'ordre des chantiers privilégie la qualité des abstractions plutôt que la
vitesse d'arrivée du multiplateforme.

## 1. Librairie de maths — terminé
- `RenderLib/Math/Math.h` : `Vec2/3/4`, `Mat4`, `Quat` POD, header-only, conformes à
  `Conventions.md`. Plus aucun DirectXMath dans le moteur hors `MathSelfCheck.cpp` (référence
  du self-check, lancé en Debug à l'init du renderer).
- Rotation du `TransformComponent` en quaternion (`SetRotationDeg` pour l'Euler), cache Euler
  dans l'inspecteur. Caméra et lumières lisent la `worldMatrix`.
- `static_assert` de layout sur `ShaderTypes.h`. `Types.h` supprimé.
- Conservés volontairement : `gWorld`, `gViewProj`, `gCameraPos` (inutilisés par le shader
  actuel, nécessaires à l'éclairage ; les retirer casserait le layout partagé avec le CPU).
- Reste pour la spec 3 : masquer DirectX à l'exe (PCH public/privé).

## 2. Prérequis de l'éditeur
Ordre : `dt` → input + caméra éditeur → sérialisation (la caméra aide à vérifier les scènes
chargées).

### `Update(dt)`
- Classe `Clock` dans RenderLib (`std::chrono::steady_clock`) : `Tick()` une fois par frame,
  puis `DeltaTime()` (borné, ex. 0,1 s, contre les pics : breakpoint, déplacement de fenêtre)
  et `TotalTime()`. Une instance possédée par `Win32Application`, ticquée en début de frame
  dans `Run` ; pas de singleton.
- Passé à la logique applicative (caméra éditeur) avant `TransformSystem::Update`. Pas de
  `IRenderer::Update` : le renderer n'en a pas besoin.
- Hors périmètre : pas de temps fixe (à ajouter avec la physique).

### Input
- État brut dans RenderLib (touches, boutons, delta souris), alimenté par le WndProc de l'exe,
  **sans dépendance à ImGui**. Remis à zéro sur `WM_KILLFOCUS`.
- Delta souris relatif via Raw Input (`WM_INPUT`).
- Le filtrage est décidé par l'application : en spec 2 via `io.WantCaptureMouse/Keyboard`
  (à exposer depuis la DLL — le retour de `HandleWindowMessage` ne suffit pas, ImGui ne
  « consomme » quasiment jamais un message) ; en spec 3 via « panneau Viewport survolé/focus ».

### Caméra éditeur
- **Hors scène** : jamais sérialisée, ne touche pas la caméra `isMain` du jeu.
- Contrôleur dans l'exe (puis `CoriumEditor/`), clic droit + WASD.
- Le renderer accepte une vue externe (ex. `Render(scene, const CameraView*)`) ; sans vue,
  il utilise la caméra `isMain` (runtime). Réutilisé par le Viewport de la spec 3.

### Sérialisation de scène
- Format texte maison, **sous-ensemble strict de YAML** documenté dans `doc/SceneFormat.md` :
  indentation 2 espaces, `clé: valeur` en bloc, listes `-`, commentaires `#`, vecteurs en
  `[x, y, z]` ; pas d'ancres ni de style flow général. `version: 1` en tête.
- Code dans `RenderLib/Scene` (runtime et éditeur). `Load(path, scene, renderer)` : le
  renderer résout les noms en index. Chargement dans une `Scene` neuve.
- Contenu du fichier :
  - `meshes:` primitives procédurales nommées (`cube` → `primitive: box`, `size: 1`), enregistrées
    avant les entités. Pas de système d'assets.
  - `materials:` `MaterialDesc` nommés, dans la scène pour l'instant (fichiers séparés quand il
    y aura un navigateur d'assets).
  - `entities:` composants avec mesh/matériau référencés **par nom**, jamais par index runtime
    (ajouter un `Find` au `MeshRegistry`).
- Identité persistante : chaque `Entity` porte un **UUID 64 bits** aléatoire (`std::mt19937_64`
  initialisé par `std::random_device`), généré à la création, conservé au chargement. C'est lui
  qui est sérialisé et qui référence le parent. L'`EntityID` runtime (`uint32_t` incrémental)
  reste la clé interne des `ComponentMap` et n'est jamais écrit ; table UUID → `EntityID`
  construite au chargement. Collision au chargement = erreur.
- Écriture **déterministe** : parcours de la hiérarchie (racines puis profondeur, ordre de
  `children`), jamais l'ordre des `unordered_map`. Parent écrit avant ses enfants → `SetParent`
  en une passe.
- Sérialisés : nom, `active`, transform (rotation en quaternion, normalisé au chargement),
  hiérarchie, mesh (`visible`, `castShadows`), caméra, lumière (`enabled`). Non sérialisés :
  `localMatrix`, `worldMatrix`, `dirty` (forcé à `true`).
- Flottants via `std::to_chars` / `std::from_chars` (aller-retour exact, indépendant de la
  locale — la locale FR lirait `1,5`).
- Erreurs : champ/composant inconnu ignoré avec avertissement, erreur de syntaxe = échec avec
  numéro de ligne. Pas de `DX12Exception`.
- Fichiers dans un dossier `Assets/` versionné à la racine, copié à côté de l'exe au build
  (lecture runtime via `GetExecutableDir()`) ; la sauvegarde éditeur écrit dans la source.
- Remplace `LoadDemoScene` dans `Main.cpp` (`Assets/Scenes/Demo.scene`).
- **Critère de fin** : self-check Debug (comme `MathSelfCheck`) charger → sauver → recharger →
  resauver, fichiers identiques octet pour octet.

## 3. Éditeur en projet séparé
- Nouveau projet `CoriumEditor/` (exe) lié à RenderLib ; `CoriumEngine/` devient le runtime sans éditeur.
- `EditorUI` sort de la DLL ; `HandleWindowMessage` sort de `IRenderer`.
- Rendu de la scène dans une cible offscreen, exposée en `ImTextureID` pour un panneau Viewport docké.
- Point à trancher dès le départ : partage du contexte ImGui entre DLL et exe
  (contexte exporté + `ImGui::SetCurrentContext`, ou ImGui compilé en DLL).
- Le backend ImGui reste dans le renderer (DX12) ; l'éditeur ne manipule que des widgets.

## 4. Abstraction RHI + backend Vulkan
- Conçue d'après les besoins réels du renderer et de l'éditeur (cibles offscreen, textures UI).
- Inversion Y de Vulkan par viewport à hauteur négative (cf. `Conventions.md`).
- À faire avant l'ajout de nouvelles features de rendu (textures, ombres, post-process).
