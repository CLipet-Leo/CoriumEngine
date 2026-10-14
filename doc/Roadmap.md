# Feuille de route — CoriumEngine

Objectif du projet : exercice d'architecture et de contraintes d'un projet de grande envergure
(moteur de jeu). L'ordre des chantiers privilégie la qualité des abstractions plutôt que la
vitesse d'arrivée du multiplateforme.

## 1. Librairie de maths
- Types POD `Vec2/3/4`, `Mat4`, `Quat` dans `RenderLib/Math`, conformes à `Conventions.md`.
- Objectif : les types de l'API (composants, constant buffers, signatures exportées) sont des
  types Corium. Plus de `XM*` ni de `using namespace DirectX` dans les headers :
  `Components.h`, `ShaderTypes.h`, `MaterialRegistry.h`, `TransformSystem.h` (`FXMMATRIX`
  exporté), ni dans `MeshVertex` (`MeshRegistry.cpp`) et `DX12Renderer.cpp`.
  (Masquer complètement DirectX à l'exe demande de séparer le PCH public/privé → spec 3.)
- Implémentation libre : maison, ou DirectXMath confiné dans un `.cpp` de `Math/`.
  Retirer de `RenderLib.h` les includes DirectX inutilisés (`PackedVector`, `Colors`, `Collision`).
- `static_assert` de layout sur les structures de `ShaderTypes.h`.
- Rotation du `TransformComponent` en quaternion. Euler uniquement dans l'inspecteur, via un
  cache côté éditeur (pas de reconversion quat → Euler à chaque frame).
- `CameraSystem` et `LightSystem` lisent la `worldMatrix` (bug actuel : position/rotation
  locales, la hiérarchie est ignorée) ; une seule construction de rotation, dans `TransformSystem`.
- `Core/Geometry/Types.h` supprimé : `Vector3f`/`Object3D` inutilisés, `Vertex3` → `Vec3`.
- Nettoyage : `CameraComponent::viewMatrix/projectionMatrix` (jamais lus), dé-transposition de
  `viewProj` dans `RenderSystem`, `gWorld`/`gViewProj`/`gCameraPos` inutilisés du shader.
- Critère de fin : un self-check à base d'`assert` (quat → mat vs `XMMatrixRotationRollPitchYaw`
  sur quelques angles) et la scène de démo rendue à l'identique.
- Ordre : `Math/` + check → `ShaderTypes`/`MeshVertex`/`Vertex3` → composants (Euler gardés)
  → quaternion + systèmes en world → nettoyage des includes.

## 2. Prérequis de l'éditeur
- Sérialisation de scène : format texte maison inspiré du YAML (à la Unity), entités et
  composants identifiés, chargement/sauvegarde. Remplace `LoadDemoScene` dans `Main.cpp`.
  Rotations sérialisées en quaternion.
- `Update(dt)` dans la boucle.
- Couche d'input minimale (caméra éditeur).

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
