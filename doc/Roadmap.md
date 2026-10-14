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
