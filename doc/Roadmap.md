# Feuille de route — CoriumEngine

Objectif du projet : exercice d'architecture et de contraintes d'un projet de grande envergure
(moteur de jeu). L'ordre des chantiers privilégie la qualité des abstractions plutôt que la
vitesse d'arrivée du multiplateforme.

## 1. Librairie de maths — terminé
- `CoriumEngine/Math/Math.h` : `Vec2/3/4`, `Mat4`, `Quat` POD, header-only, conformes à
  `Conventions.md`. Plus aucun DirectXMath dans le moteur hors `MathSelfCheck.cpp` (référence
  du self-check, lancé en Debug à l'init du renderer).
- Rotation du `TransformComponent` en quaternion (`SetRotationDeg` pour l'Euler), cache Euler
  dans l'inspecteur. Caméra et lumières lisent la `worldMatrix`.
- `static_assert` de layout sur `ShaderTypes.h`. `Types.h` supprimé.
- Conservés volontairement : `gWorld`, `gViewProj`, `gCameraPos` (inutilisés par le shader
  actuel, nécessaires à l'éclairage ; les retirer casserait le layout partagé avec le CPU).
- DirectX masqué à l'exe (PCH public/privé) : fait en prérequis de la spec 3.

## 2. Prérequis de l'éditeur — terminé
Ordre : `dt` → input + caméra éditeur → sérialisation (la caméra aide à vérifier les scènes
chargées).

Fait : `Clock` et `Input` (`CoriumEngine/Core/Platform`), `EditorCamera` (exe),
`Render(scene, const CameraView*)`, `IRenderer::WantCaptureMouse`, UUID d'entité,
`SceneSerializer` + `doc/SceneFormat.md`, `Assets/Scenes/Demo.scene` (remplace `LoadDemoScene`),
self-check Debug passé. Pas encore de commande « Sauver » dans l'UI (l'API `Save` existe).

### `Update(dt)`
- Classe `Clock` dans la DLL (`std::chrono::steady_clock`) : `Tick()` une fois par frame,
  puis `DeltaTime()` (borné, ex. 0,1 s, contre les pics : breakpoint, déplacement de fenêtre)
  et `TotalTime()`. Une instance possédée par `Win32Application`, ticquée en début de frame
  dans `Run` ; pas de singleton.
- Passé à la logique applicative (caméra éditeur) avant `TransformSystem::Update`. Pas de
  `IRenderer::Update` : le renderer n'en a pas besoin.
- Hors périmètre : pas de temps fixe (à ajouter avec la physique).

### Input
- État brut dans la DLL (touches, boutons, delta souris), alimenté par le WndProc de l'exe,
  **sans dépendance à ImGui**. Remis à zéro sur `WM_KILLFOCUS`.
- Delta souris relatif via Raw Input (`WM_INPUT`).
- Le filtrage est décidé par l'application : en spec 2 via `io.WantCaptureMouse/Keyboard`
  (à exposer depuis la DLL — le retour de `HandleWindowMessage` ne suffit pas, ImGui ne
  « consomme » quasiment jamais un message) ; en spec 3 via « panneau Viewport survolé/focus »
  (cf. spec 3, Viewport offscreen).

### Caméra éditeur
- **Hors scène** : jamais sérialisée, ne touche pas la caméra `isMain` du jeu.
- Contrôleur dans l'exe (puis `CoriumEditor/`), clic droit + ZQSD (A/E : bas/haut).
- Le renderer accepte une vue externe (ex. `Render(scene, const CameraView*)`) ; sans vue,
  il utilise la caméra `isMain` (runtime). Réutilisé par le Viewport de la spec 3.

### Sérialisation de scène
- Format texte maison, **sous-ensemble strict de YAML** documenté dans `doc/SceneFormat.md` :
  indentation 2 espaces, `clé: valeur` en bloc, listes `-`, commentaires `#`, vecteurs en
  `[x, y, z]` ; pas d'ancres ni de style flow général. `version: 1` en tête.
- Code dans `CoriumEngine/Scene` (runtime et éditeur). `Load(path, scene, renderer)` : le
  renderer résout les noms en index. Chargement dans une `Scene` neuve.
- Contenu du fichier :
  - `meshes:` primitives procédurales nommées (`cube` → `primitive: box`, `size: 1`), enregistrées
    avant les entités. Pas de système d'assets.
  - `materials:` `MaterialDesc` nommés, dans la scène pour l'instant (fichiers séparés quand il
    y aura un navigateur d'assets).
  - `entities:` composants avec mesh/matériau référencés **par nom**, jamais par index runtime
    (table nom → index propre au fichier, construite au chargement ; `RegisterMesh` prend un
    `MeshDesc` pour pouvoir réécrire le mesh).
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

## 3. Éditeur en projet séparé — terminé
Fait : `CoriumEditor/` (exe), runtime `CoriumRuntime/` sans ImGui, `EditorUI`/`EditorCamera` dans
l'éditeur, contexte ImGui possédé par l'éditeur (cf. ci-dessous), Viewport offscreen docké et
redimensionnable, layout par défaut (DockBuilder) puis persistant. Vérifié Debug (couche de debug
+ GBV, arrêt sur erreur) : redimensionnements, Viewport détaché en fenêtre OS, caméra au survol.
Écart au plan : le contexte est créé par l'éditeur, pas par la DLL — l'ordre d'arrêt imposé par
le multi-viewport (DX12 avant Win32, contexte en dernier) devient un simple ordre inverse.
Reste : synchro entre la queue principale et celles des fenêtres détachées (un Viewport détaché
peut lire la cible pendant son écriture, déchirure possible) ; commande « Sauver ».

- Nouveau projet `CoriumEditor/` (exe) lié à la DLL ; l'exe actuel devient le runtime sans
  éditeur ni ImGui (`Render(scene, nullptr)`, caméra `isMain`, rendu direct dans la swapchain).
  `EditorCamera` part dans `CoriumEditor/`.
- `EditorUI` sort de la DLL (ne dépend que de `Scene`, déjà exportée) ; les stats (index de frame,
  nom de l'adaptateur) passent par une petite API (ex. `IRenderer::GetStats()`).
- Rendu de la scène dans une cible offscreen, exposée en `ImTextureID` pour un panneau Viewport docké.

### Prérequis (dans l'ordre)
1. ~~Réparer la config `Release`~~ (fait : alignée sur `Debug_DX12` ; config `Debug` orpheline
   supprimée).
2. ~~Masquer DirectX aux exe~~ (fait : `stdafx.h` n'inclut que `CorePublic.h` ; `CameraView`
   déplacé dans `Renderer.h`). Le header `Core.h` reste le PCH interne de la DLL.
3. ~~Fenêtre Win32 + boucle déplacées dans `CoriumEngine/Core/Platform`~~ (fait : `Win32Application`
   exportée, l'exe dérive et surcharge `Update(dt)`/`GetView()` ; titre et icône passés par l'exe).
   Hook de message : `Win32Application::OnWindowMessage` (remplace `IRenderer::HandleWindowMessage`).

### Contexte ImGui (tranché)
- ImGui vcpkg est une **lib statique** (pas de DLL) : chaque module a sa copie de `GImGui`.
  ImGui en DLL écarté (impose de sortir ImGui de vcpkg).
- Le contexte est créé par l'éditeur (révisé, cf. ci-dessus) ; `InitUI` fait côté DLL
  `SetAllocatorFunctions` (allocateurs de l'exe) + `SetCurrentContext`, et `IMGUI_CHECKVERSION()`
  comme garde-fou (même manifeste vcpkg → même version et même `imconfig`).
- Backends séparés selon leur nature : **Win32** (plateforme) dans l'éditeur, qui possède la
  fenêtre et le WndProc — d'où la sortie de `HandleWindowMessage` d'`IRenderer` ; **DX12** reste
  dans le renderer. Le multi-viewport passe par les callbacks `PlatformIO` du contexte partagé.
- Init ImGui optionnelle côté renderer (`InitUI`/`ShutdownUI`), appelée seulement par l'éditeur.
- `ImTextureID` = `ImU64` (ImGui ≥ 1.91.4) : `D3D12_GPU_DESCRIPTOR_HANDLE.ptr` sans type DX12 exposé.

### Pilotage de la frame
- `Render` ne peut plus appeler l'UI lui-même. Déroulé :
  `BeginUIFrame()` (NewFrame) → `editor.Draw(...)` (widgets, dont `Image(viewportTex)`) →
  `Render(scene, &view)` (scène → offscreen, barrière RT → SRV, ImGui → swapchain, Present).

### Viewport offscreen
- Aspect calculé depuis la taille de la cible (le panneau), plus depuis la swapchain.
- Format identique à la swapchain (`R8G8B8A8_UNORM`), sinon ajouter le format à la clé de PSO.
- Taille connue pendant `Draw` : redimensionnement appliqué en début de `Render`, avec
  `FlushGpu` (pas de destruction de texture ni réécriture de slot SRV en vol). Slot SRV dédié
  (slot 0 = police ImGui).
- Filtrage de l'input (cf. spec 2) : `WantCaptureMouse` est toujours vrai au-dessus du Viewport
  (c'est une fenêtre ImGui) → la caméra éditeur se base sur `IsWindowHovered/Focused` du panneau.

### Build
- `CopyFileToFolders` (shaders, `Assets/`) aussi pour `CoriumEditor`. Sortie partagée
  `Build/<Config>/` (un seul `imgui.ini`, sans conflit tant que le runtime n'a pas d'UI).

### Critère de fin
- `CoriumRuntime.exe` ne lie pas ImGui ; l'éditeur affiche le Viewport docké et redimensionnable ;
  la caméra ne réagit qu'au survol du Viewport ; layout persistant ; aucune erreur de la couche
  de debug DX12 pendant un redimensionnement.

## 4. Abstraction RHI + backend Vulkan
À faire avant les nouvelles features de rendu (ombres, post-process) : chaque feature écrite
en DX12 pur serait à réécrire. RHI et Vulkan dans le même chantier : une RHI à un seul backend
finit en enveloppe de DX12, le second backend force l'abstraction à rester honnête.
Plateforme toujours Windows x64 : le gain est architectural, pas de portage Linux ici.

Références (lecture, pas dépendance — l'exercice est d'écrire la RHI) : **nvrhi** (NVIDIA,
MIT) — `include/nvrhi/nvrhi.h` pour la forme d'une interface RHI, `BindingLayout`/`BindingSet`
pour la traduction root signature → descriptor sets, suivi automatique des états à comparer
avec nos barrières explicites ; ShaderMake pour la compilation HLSL → DXIL/SPIR-V.
GLFW écarté : `Win32Application`/`Input` couvrent déjà la plateforme (à revoir avec Linux).

### Prérequis (dans l'ordre)
1. ~~Synchro multi-viewport (reliquat de la spec 3) corrigée en DX12~~ (fait : avant de rendre les
   fenêtres détachées, attente CPU de la fence de frame ; le backend ImGui crée une queue par
   fenêtre, inaccessible pour un `Wait` GPU. Seulement quand une fenêtre est détachée).
2. ~~Code GPU sorti de `Scene/`~~ (fait : `MeshRegistry`, `MaterialRegistry`, `RenderSystem` dans
   `CoriumEngine/Renderer/` ; `MeshDesc`/`MaterialDesc` et leurs enums, sans type GPU, restent
   dans `Scene/ResourceDescs.h`).
3. ~~Shaders compilés au build par **DXC**~~ (fait : `directx-dxc` vcpkg, cible MSBuild
   `CompileShaders` après `ClCompile` ; SM 6.0, `Shaders/Basic_vs|ps.cso|spv`, chargés par
   `D3DReadFileToBlob`. SPIR-V avec le mapping de registres par défaut, à fixer avec le modèle
   de binding).

### Périmètre de la RHI
- Conçue d'après les besoins réels, et pas plus : buffers (vertex, index, constantes),
  textures (échantillonnées, render target, depth), sampler, pipeline graphique (clé de PSO
  actuelle), command list, swapchain, synchro par frame en vol, cible offscreen exposée à l'UI.
- Pas de compute, pas de render graph, pas de multi-queue dans cette spec.
- Interfaces virtuelles internes à la DLL (`Rhi*`), jamais exportées : l'API publique reste
  `IRenderer`. Le renderer (`Renderer`, ex-`DX12Renderer`) ne connaît que la RHI ; `DX12*` et
  `Vk*` ne sont inclus que par leur backend.
- Barrières : états de ressource explicites et simples (`RenderTarget`, `ShaderRead`,
  `CopyDest`, `Present`, `Depth`), traduits par chaque backend. Pas de suivi automatique.
- Choix du backend **au runtime** : `CreateRenderer(RendererBackend)` (+ argument de ligne de
  commande `-vulkan`/`-dx12`, DX12 par défaut). Les deux backends dans la même DLL, mêmes
  configs de build (`Debug_DX12` renommée `Debug` quand Vulkan arrive).

### Modèle de binding (à trancher en premier)
Le point dur : la root signature actuelle (CBV root descriptors `b0` objet, `b1` scène,
`b2` lumières) n'a pas d'équivalent direct en Vulkan.
- Proposé : fréquences de mise à jour = espaces de registre = descriptor sets.
  `space0` par frame (scène, lumières), `space1` par matériau (textures, sampler, constantes),
  `space2` par objet (constantes de l'objet).
- Constantes par objet : buffer par frame + offset dynamique (DX12 : root CBV à l'adresse
  + offset ; Vulkan : `UNIFORM_BUFFER_DYNAMIC`). Pas de push constants/root constants pour
  l'instant (limite de 128 octets sur certains GPU, la `worldMatrix` + normale les dépasse vite).
- Mapping HLSL → SPIR-V par options DXC (`-fvk-b-shift`, `-fvk-t-shift`…) ou `[[vk::binding(n, set)]]` :
  un seul fichier HLSL, pas de `#ifdef` par backend.
- Pas de bindless dans cette spec (à reconsidérer avec un vrai système d'assets).

### Texture de validation
Une texture échantillonnée dans le matériau (damier procédural généré au chargement, pas de
loader d'image) : impose SRV/descriptor set, sampler, upload par staging et barrières, sur les
deux backends. C'est le cas d'usage qui valide le modèle de binding ; pas une feature.

### Backend Vulkan
- Vulkan 1.3 (dynamic rendering, synchronization2) : pas de `VkRenderPass`/`VkFramebuffer`.
- Loader : `volk` ; mémoire : **VMA** (vcpkg). Swapchain : `VkSurfaceKHR` sur le `HWND`
  de `Win32Application`, recréée au resize comme la swapchain DX12.
- Inversion Y par viewport à hauteur négative (cf. `Conventions.md`). Le sens des faces vu
  par le rasterizer s'inverse avec le viewport : vérifier que le culling reste identique au
  DX12 (`FrontCounterClockwise = FALSE`) et consigner le réglage Vulkan dans `Conventions.md`.
- Profondeur [0, 1] native, rien à faire.
- ImGui : `imgui_impl_vulkan` (feature vcpkg `vulkan-binding`), multi-viewport compris.
  `ImTextureID` = `VkDescriptorSet` (64 bits) : `GetSceneTexture()` ne change pas.
- Debug : validation layers + synchronization validation, arrêt sur erreur (équivalent de la
  couche de debug + GBV actuelle).

### Critère de fin
- Aucun type `D3D12`/`DXGI`/`Vk` hors de `Core/DX12` et `Core/Vulkan`.
- `Demo.scene` (avec la texture de validation) rendue à l'identique par les deux backends
  (comparaison visuelle runtime et éditeur).
- Éditeur fonctionnel sur les deux backends : Viewport docké, redimensionnement, Viewport
  détaché, caméra.
- Zéro erreur de la couche de debug DX12 (GBV) et des validation layers Vulkan (synchro
  comprise) pendant redimensionnements et détachement de fenêtre.
